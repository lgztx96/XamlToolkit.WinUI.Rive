#include "pch.h"
#include "winrt_module_imports.h"
#include "RiveRenderer.h"

namespace winrt::XamlToolkit::WinUI::Rive::implementation
{
    using namespace std::chrono_literals;

    namespace
    {
        using winrt::Microsoft::UI::Composition::ICompositionDrawingSurfaceInterop;
        using winrt::Microsoft::UI::Composition::ICompositionGraphicsDevice2;
        using winrt::Microsoft::UI::Composition::ICompositionSurface;
        using winrt::Microsoft::UI::Composition::ICompositorInterop;
        using winrt::Microsoft::UI::Xaml::Hosting::ElementCompositionPreview;

        // Rive treats the alpha byte of the clear color as a multiplier of the
        // color channels (see UnpackColorToRGBA32FPremul), so an alpha of zero
        // clears to transparent black and the artboard renders over whatever is
        // behind the control - which is what artboards without a background of
        // their own expect. The surface is premultiplied, so transparent black
        // is the correct "nothing here" value. Use 0xFF404040 for an opaque
        // background instead.
        constexpr uint32_t kClearColor = 0x00000000;

        // A rebuild is retried a few times before giving up, so a device that
        // cannot be recreated does not spin in an endless rebuild loop.
        constexpr int kMaxReinitializeAttempts = 3;

        // ~60 fps, and the largest delta a single frame may advance the scene
        // by. The cap only ever applies after a stall of several frames.
        constexpr auto kFrameInterval = std::chrono::milliseconds(16);
        constexpr float kMaxFrameDelta = 0.1f;

        // The body is debug only, so in release builds neither parameter is
        // referenced and the compiler would otherwise warn about both.
        void LogHresult([[maybe_unused]] wchar_t const* message, [[maybe_unused]] HRESULT hr) noexcept
        {
#ifdef _DEBUG

            wchar_t buffer[256]{};
            swprintf_s(buffer, L"[XamlToolkit.WinUI.Rive] %s (0x%08X)\n", message, static_cast<unsigned>(hr));
            OutputDebugStringW(buffer);
#endif
        }
    }

    RiveRenderer::RiveRenderer() = default;

    RiveRenderer::~RiveRenderer()
    {
        Stop();
        ReleaseFrameResources();
        ReleaseDeviceResources();
        ReleaseCompositionResources();
    }

#pragma region Threads

    void RiveRenderer::Start()
    {
        if (!_renderThread)
        {
            _lastFrameTime = std::chrono::high_resolution_clock::now();
            _renderThread = std::make_unique<std::jthread>(std::bind_front(&RiveRenderer::RenderLoop, this));
        }
    }

    void RiveRenderer::Stop()
    {
        if (_renderThread)
        {
            _renderThread->request_stop();
            _renderThread.reset();
        }
    }

    void RiveRenderer::Pause() { _paused = true; }
    void RiveRenderer::Resume() { _paused = false; }

    void RiveRenderer::RenderLoop(std::stop_token token)
    {
        auto nextFrame = std::chrono::steady_clock::now();
        _lastFrameTime = std::chrono::high_resolution_clock::now();

        while (!token.stop_requested())
        {
            ProcessCommands();

            const auto now = std::chrono::high_resolution_clock::now();
            const auto elapsed = std::chrono::duration<float>(now - _lastFrameTime).count();
            _lastFrameTime = now;

            if (!_paused.load())
            {
                const float dt = std::min(elapsed, kMaxFrameDelta);

                if (!TryRenderFrame(dt))
                {
                    QueueReinitialize();
                }
            }

            nextFrame += kFrameInterval;
            if (nextFrame < std::chrono::steady_clock::now())
            {
                nextFrame = std::chrono::steady_clock::now();
            }
            std::this_thread::sleep_until(nextFrame);
        }
    }
#pragma endregion

#pragma region CompositionSurface

    bool RiveRenderer::Attach(winrt::Microsoft::UI::Xaml::UIElement const& host)
    {
        if (!host)
        {
            return false;
        }

        try
        {
            if (_host && _host != host)
            {
                ReleaseCompositionResources();
                _host = nullptr;
            }
            _host = host;

            {
                std::lock_guard lock(_surfaceMutex);
                _uiDispatcher = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
            }

            CreateDeviceResources();
            CreateCompositionResources();
            UpdateSurface();

            _deviceLost = false;
            _reinitializeAttempts = 0;
            return _surfaceInterop != nullptr;
        }
        catch (winrt::hresult_error const& ex)
        {
            LogHresult(L"attaching to the host element failed", ex.code().value);
            return false;
        }
    }

    void RiveRenderer::UpdateSurface()
    {
        if (!_host)
        {
            return;
        }

        float scale = 1.0f;
        if (auto xamlRoot = _host.XamlRoot())
        {
            scale = static_cast<float>(xamlRoot.RasterizationScale());
        }
        if (!(scale > 0.0f))
        {
            scale = 1.0f;
        }

        const auto dipSize = _host.ActualSize();
        int32_t pixelWidth = static_cast<int32_t>(std::lround(static_cast<double>(dipSize.x) * scale));
        int32_t pixelHeight = static_cast<int32_t>(std::lround(static_cast<double>(dipSize.y) * scale));
        pixelWidth = std::max(pixelWidth, 1);
        pixelHeight = std::max(pixelHeight, 1);

        _rasterizationScale = scale;

        try
        {
            std::lock_guard lock(_surfaceMutex);

            if (_surfaceInterop && (pixelWidth != _surfacePixelWidth || pixelHeight != _surfacePixelHeight))
            {
                winrt::check_hresult(_surfaceInterop->Resize(SIZE{ pixelWidth, pixelHeight }));
                _surfacePixelWidth = pixelWidth;
                _surfacePixelHeight = pixelHeight;
                _layoutDirty = true;
                ClearSurface();
            }

            if (_surfaceVisual)
            {
                _surfaceVisual.Size({ dipSize.x, dipSize.y });
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            LogHresult(L"resizing the drawing surface failed", ex.code().value);
            QueueReinitialize();
        }
    }

    void RiveRenderer::Detach()
    {
        ReleaseCompositionResources();
        _host = nullptr;
        {
            std::lock_guard lock(_surfaceMutex);
            _uiDispatcher = nullptr;
        }
    }

    void RiveRenderer::CreateDeviceResources()
    {
        if (_device)
        {
            return;
        }

        winrt::check_hresult(D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
            D3D11_SDK_VERSION, _device.put(), nullptr, _context.put()));

        winrt::com_ptr<ID3D10Multithread> multithread;
        if (FAILED(_device->QueryInterface(IID_ID3D10Multithread, multithread.put_void())))
        {
            _context->QueryInterface(IID_ID3D10Multithread, multithread.put_void());
        }
        if (multithread)
        {
            multithread->SetMultithreadProtected(TRUE);
        }

        winrt::check_hresult(_device->QueryInterface(__uuidof(IDXGIDevice), _dxgiDevice.put_void()));

        _renderContext = rive::gpu::RenderContextD3DImpl::MakeContext(_device.get(), _context.get(), {});
        _renderer = std::make_unique<rive::RiveRenderer>(_renderContext.get());
    }

    void RiveRenderer::CreateCompositionResources()
    {
        if (_surfaceVisual || !_host)
        {
            return;
        }

        auto compositor = ElementCompositionPreview::GetElementVisual(_host).Compositor();

        auto compositorInterop = compositor.as<ICompositorInterop>();
        winrt::Microsoft::UI::Composition::ICompositionGraphicsDevice graphicsDevice{ nullptr };
        winrt::check_hresult(compositorInterop->CreateGraphicsDevice(_dxgiDevice.get(), &graphicsDevice));
        _graphicsDevice = graphicsDevice;

        // A 1x1 surface to start with: UpdateSurface() sizes it to the host
        // (resizing reallocates the surface in place, so the brush below keeps
        // pointing at the same surface object for the life of the control).
        _surface = _graphicsDevice.as<ICompositionGraphicsDevice2>().CreateDrawingSurface2(
            winrt::Windows::Graphics::SizeInt32{ 1, 1 },
            winrt::Microsoft::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            winrt::Microsoft::Graphics::DirectX::DirectXAlphaMode::Premultiplied);

        {
            std::lock_guard lock(_surfaceMutex);
            _surfaceInterop = _surface.as<ICompositionDrawingSurfaceInterop>();
            _surfacePixelWidth = 1;
            _surfacePixelHeight = 1;
            ClearSurface();
        }

        _surfaceBrush = compositor.CreateSurfaceBrush(_surface.as<ICompositionSurface>());

        // The surface is attached as a child visual of the host element, which
        // draws it clipped to the element and behind its content.
        _surfaceVisual = compositor.CreateSpriteVisual();
        _surfaceVisual.Brush(_surfaceBrush);
        _surfaceVisual.Size({ 1.0f, 1.0f });
        ElementCompositionPreview::SetElementChildVisual(_host, _surfaceVisual);
    }

    void RiveRenderer::ClearSurface()
    {
        // A freshly created or resized surface holds undefined content, so clear
        // it to nothing until the first frame is drawn into it. Called with the
        // surface lock held.
        winrt::com_ptr<ID3D11Texture2D> surfaceTexture;
        POINT updateOffset{};
        if (FAILED(_surfaceInterop->BeginDraw(nullptr, __uuidof(ID3D11Texture2D), surfaceTexture.put_void(), &updateOffset)))
        {
            return;
        }

        D3D11_TEXTURE2D_DESC surfaceDesc{};
        surfaceTexture->GetDesc(&surfaceDesc);

        D3D11_RENDER_TARGET_VIEW_DESC viewDesc{};
        viewDesc.Format = surfaceDesc.Format;
        viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        switch (surfaceDesc.Format)
        {
        case DXGI_FORMAT_B8G8R8A8_TYPELESS: viewDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; break;
        case DXGI_FORMAT_R8G8B8A8_TYPELESS: viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; break;
        default: break;
        }

        winrt::com_ptr<ID3D11RenderTargetView> renderTargetView;
        if (SUCCEEDED(_device->CreateRenderTargetView(surfaceTexture.get(), &viewDesc, renderTargetView.put())))
        {
            // Only the update rect belongs to this surface. The texture BeginDraw
            // hands back is a slice of a shared atlas that carries the surface's
            // other buffers - BeginDraw returns a different offset every frame -
            // and other surfaces besides. ClearRenderTargetView would wipe the
            // whole texture, so clear the sub-rect instead.
            const LONG left = std::max<LONG>(updateOffset.x, 0);
            const LONG top = std::max<LONG>(updateOffset.y, 0);
            const LONG right = std::min<LONG>(left + _surfacePixelWidth, static_cast<LONG>(surfaceDesc.Width));
            const LONG bottom = std::min<LONG>(top + _surfacePixelHeight, static_cast<LONG>(surfaceDesc.Height));

            const float transparent[4]{ 0.0f, 0.0f, 0.0f, 0.0f };
            winrt::com_ptr<ID3D11DeviceContext1> context1;
            if (right > left && bottom > top &&
                SUCCEEDED(_context->QueryInterface(__uuidof(ID3D11DeviceContext1), context1.put_void())))
            {
                const D3D11_RECT rect{ left, top, right, bottom };
                context1->ClearView(renderTargetView.get(), transparent, &rect, 1);
            }
            _context->Flush();
        }

        _surfaceInterop->EndDraw();
    }

    void RiveRenderer::ReleaseCompositionResources()
    {
        if (!_surfaceInterop && !_surfaceVisual)
        {
            return;
        }

        {
            std::lock_guard lock(_surfaceMutex);
            _surfaceInterop = nullptr;
            _surfacePixelWidth = 0;
            _surfacePixelHeight = 0;
        }

        try
        {
            if (_host)
            {
                ElementCompositionPreview::SetElementChildVisual(_host, nullptr);
            }
        }
        catch (winrt::hresult_error const&)
        {
   
        }

        _surfaceVisual = nullptr;
        _surfaceBrush = nullptr;
        _surface = nullptr;
        _graphicsDevice = nullptr;
    }

    void RiveRenderer::ReleaseFrameResources()
    {
        _renderTarget.reset();
        _frameTexture = nullptr;
        _frameWidth = 0;
        _frameHeight = 0;
        _frameFormat = DXGI_FORMAT_UNKNOWN;
        _layoutDirty = true;
    }

    void RiveRenderer::ReleaseDeviceResources()
    {
        ReleaseFrameResources();
        _renderer.reset();
        _renderContext.reset();
        _dxgiDevice = nullptr;
        _context = nullptr;
        _device = nullptr;
    }

    void RiveRenderer::EnsureFrameResources(int32_t width, int32_t height, DXGI_FORMAT format)
    {
        if (_frameTexture && _frameWidth == width && _frameHeight == height && _frameFormat == format)
        {
            return;
        }

        _renderTarget.reset();
        _frameTexture = nullptr;
        _frameWidth = 0;
        _frameHeight = 0;
        _frameFormat = DXGI_FORMAT_UNKNOWN;

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(width);
        desc.Height = static_cast<UINT>(height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        // A render target is all this texture is: Rive never binds it as a UAV
        // and never samples it, it only ever writes it and it is read back with
        // CopySubresourceRegion. Which of Rive's two color paths that write takes
        // - clearing and drawing straight into this texture's render target view,
        // or resolving into it from Rive's own offscreen UAV - is decided by
        // wants_fixed_function_color_output() in render_context.cpp from the
        // platform's features, not by these bind flags.
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;

        if (FAILED(_device->CreateTexture2D(&desc, nullptr, _frameTexture.put())))
        {
            _frameTexture = nullptr;
            return;
        }

        _renderTarget = _renderContext->static_impl_cast<rive::gpu::RenderContextD3DImpl>()->makeRenderTarget(width, height);
        if (!_renderTarget)
        {
            _frameTexture = nullptr;
            return;
        }

        _frameWidth = width;
        _frameHeight = height;
        _frameFormat = format;
        _layoutDirty = true;
    }

    bool RiveRenderer::TryRenderFrame(float dt)
    {
        if (!_artboard || !_scene || !_renderContext || _deviceLost.load())
        {
            return true;
        }

        // Held for the whole frame: the surface stays alive and unmoved until the
        // finished frame has been handed back.
        std::unique_lock lock(_surfaceMutex);
        if (!_surfaceInterop)
        {
            return true;
        }

        winrt::com_ptr<ID3D11Texture2D> surfaceTexture;
        POINT updateOffset{};
        HRESULT hr = _surfaceInterop->BeginDraw(nullptr, __uuidof(ID3D11Texture2D), surfaceTexture.put_void(), &updateOffset);
        if (FAILED(hr))
        {
            LogHresult(L"BeginDraw failed", hr);
            return false;
        }

        D3D11_TEXTURE2D_DESC surfaceDesc{};
        surfaceTexture->GetDesc(&surfaceDesc);
        const DXGI_FORMAT format = surfaceDesc.Format;

        // The texture BeginDraw hands back is the surface's backing allocation,
        // which is usually larger than the surface itself: the compositor pads
        // it, and only the region [updateOffset, updateOffset + surfaceSize)
        // actually maps onto the visual. The size that matters is therefore the
        // one Resize() was asked for, never the backing texture's. Rendering at
        // the backing size would lay the artboard out against the padded rect
        // and let the compositor show just the top left corner of it.
        const int32_t width = _surfacePixelWidth;
        const int32_t height = _surfacePixelHeight;

        const UINT offsetX = static_cast<UINT>(std::max(updateOffset.x, 0L));
        const UINT offsetY = static_cast<UINT>(std::max(updateOffset.y, 0L));
        if (width <= 0 || height <= 0 ||
            offsetX + static_cast<UINT>(width) > surfaceDesc.Width ||
            offsetY + static_cast<UINT>(height) > surfaceDesc.Height)
        {
            LogHresult(L"drawing surface is smaller than the size it was asked for", E_UNEXPECTED);
            _surfaceInterop->EndDraw();
            return true;
        }

        const bool formatSupported = format == DXGI_FORMAT_B8G8R8A8_UNORM ||
                                     format == DXGI_FORMAT_R8G8B8A8_UNORM ||
                                     format == DXGI_FORMAT_B8G8R8A8_TYPELESS ||
                                     format == DXGI_FORMAT_R8G8B8A8_TYPELESS;
        if (!formatSupported)
        {
            LogHresult(L"unsupported drawing surface format", static_cast<HRESULT>(format));
            _surfaceInterop->EndDraw();
            return true;
        }

        EnsureFrameResources(width, height, format);
        if (!_frameTexture || !_renderTarget)
        {
            _surfaceInterop->EndDraw();
            return false;
        }

        if (_layoutDirty.exchange(false))
        {
            _viewTransform = rive::computeAlignment(
                rive::Fit::contain,
                rive::Alignment::center,
                rive::AABB{ 0, 0, static_cast<float>(width), static_cast<float>(height) },
                _artboard->bounds());
        }
        _renderTarget->setTargetTexture(_frameTexture.get());
        _renderContext->beginFrame({ .renderTargetWidth = static_cast<uint32_t>(width),
                                     .renderTargetHeight = static_cast<uint32_t>(height),
                                     .clearColor = kClearColor });

        _scene->advanceAndApply(dt);

        _renderer->save();
        _renderer->transform(_viewTransform);
        _artboard->draw(_renderer.get());
        _renderer->restore();

        // Every bit of GPU work for this frame - the clear included - targets our
        // own texture. Rive issues no copy of its own and no flush, so nothing
        // reaches the surface until the finished frame is copied below.

        _renderContext->flush({ .renderTarget = _renderTarget.get() });
        // Handing the texture over and taking it back is part of the frame
        // contract, not a caching convenience: nullptr is what releases the
        // render target view and UAV that setTargetTexture() dropped, and Rive's
        // own D3D11 host brackets every flush the same way (see
        // fiddle_context_d3d.cpp: setTargetTexture(backbuffer) before flush,
        // setTargetTexture(nullptr) once the frame is done). Holding those views
        // across frames instead takes this driver down within seconds.
        _renderTarget->setTargetTexture(nullptr);

        // Copy exactly the surface's region: the backing texture can be larger
        // than the frame, and a CopyResource would either be rejected outright
        // (mismatched sizes) or spill the padded area into the surface.
        const D3D11_BOX box{ 0, 0, 0, static_cast<UINT>(width), static_cast<UINT>(height), 1 };
        _context->CopySubresourceRegion(surfaceTexture.get(), 0,
            offsetX, offsetY, 0,
            _frameTexture.get(), 0, &box);

        // Submit the copy before handing the surface back: the compositor may
        // sample it as soon as EndDraw returns.
        _context->Flush();

        hr = _surfaceInterop->EndDraw();
        if (FAILED(hr))
        {
            LogHresult(L"EndDraw failed", hr);
            return false;
        }

        return true;
    }
#pragma endregion

#pragma region DeviceLost

    void RiveRenderer::QueueReinitialize()
    {
        if (_deviceLost.exchange(true))
        {
            return;
        }

        if (_reinitializeAttempts >= kMaxReinitializeAttempts)
        {
            OutputDebugStringW(L"[XamlToolkit.WinUI.Rive] giving up on rebuilding the graphics device\n");
            return;
        }

        winrt::Microsoft::UI::Dispatching::DispatcherQueue dispatcher{ nullptr };
        {
            std::lock_guard lock(_surfaceMutex);
            dispatcher = _uiDispatcher;
        }

        if (!dispatcher)
        {
            return;
        }

        std::weak_ptr<RiveRenderer> weak = weak_from_this();
        if (!dispatcher.TryEnqueue([weak]()
            {
                if (auto self = weak.lock())
                {
                    self->Reinitialize();
                }
            }))
        {
            _deviceLost = false;
        }
    }

    void RiveRenderer::Reinitialize()
    {
        _deviceLost = false;

        try
        {
            Stop();

            ReleaseFrameResources();
            ReleaseDeviceResources();
            ReleaseCompositionResources();

            if (!_host)
            {
                return;
            }

            CreateDeviceResources();
            CreateCompositionResources();
            UpdateSurface();

            OutputDebugStringW(L"[XamlToolkit.WinUI.Rive] rebuilt the graphics device\n");
            _reinitializeAttempts = 0;
            Start();
        }
        catch (winrt::hresult_error const& ex)
        {
            LogHresult(L"rebuilding the graphics device failed", ex.code().value);
            ++_reinitializeAttempts;
            QueueReinitialize();
        }
    }
#pragma endregion

#pragma region Scene

    void RiveRenderer::ClearCommands()
    {
        std::lock_guard<std::mutex> lock(_commandsMutex);
        while (!_commands.empty())
        {
            _commands.pop();
        }
    }

    void RiveRenderer::LoadFileData(std::vector<uint8_t> data) { Enqueue(LoadRivCommand{ std::move(data) }); }
    void RiveRenderer::SelectArtboard(std::string name) { Enqueue(SelectArtboardCmd{ std::move(name) }); }
    void RiveRenderer::SelectStateMachine(std::string name) { Enqueue(SelectStateMachineCmd{ std::move(name) }); }

    void RiveRenderer::SetBoolInput(std::string n, bool v)
    {
        Enqueue(InputCommand{ .name = std::move(n), .kind = InputCommand::Kind::Bool, .boolValue = v });
    }
    void RiveRenderer::SetNumberInput(std::string n, float v)
    {
        Enqueue(InputCommand{ .name = std::move(n), .kind = InputCommand::Kind::Number, .numberValue = v });
    }
    void RiveRenderer::FireTrigger(std::string n)
    {
        Enqueue(InputCommand{ .name = std::move(n), .kind = InputCommand::Kind::Trigger });
    }

    void RiveRenderer::PointerMove(float x, float y) { Enqueue(PointerCommand{ x, y, PointerCommand::Kind::Move }); }
    void RiveRenderer::PointerDown(float x, float y) { Enqueue(PointerCommand{ x, y, PointerCommand::Kind::Down }); }
    void RiveRenderer::PointerUp(float x, float y) { Enqueue(PointerCommand{ x, y, PointerCommand::Kind::Up }); }
    void RiveRenderer::PointerExit(float x, float y) { Enqueue(PointerCommand{ x, y, PointerCommand::Kind::Exit }); }

    void RiveRenderer::Enqueue(Command&& cmd)
    {
        std::unique_lock<std::mutex> lock(_commandsMutex);
        _commands.emplace(std::move(cmd));
    }

    void RiveRenderer::ProcessCommands()
    {
        std::queue<Command> localQueue;
        {
            std::unique_lock<std::mutex> lock(_commandsMutex);
            localQueue.swap(_commands);
        }

        while (!localQueue.empty())
        {
            const auto& cmd = localQueue.front();

            std::visit([this](auto&& c) {
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<T, LoadRivCommand>)
                {
                    CreateRiveFile(c.data);
                }
                else if constexpr (std::is_same_v<T, SelectArtboardCmd>)
                {
                    if (_rivFile)
                    {
                        _artboard = _rivFile->artboardNamed(c.name);
                        if (_artboard) OnArtboardChanged();
                    }
                }
                else if constexpr (std::is_same_v<T, SelectStateMachineCmd>)
                {
                    if (_artboard)
                    {
                        if (auto sm = _artboard->stateMachineNamed(c.name))
                        {
                            // Taken before the move: moving the unique_ptr does
                            // not move what it points at.
                            _activeStateMachine = sm.get();
                            _scene = std::move(sm);
                            _layoutDirty = true;
                        }
                    }
                }
                else if constexpr (std::is_same_v<T, PointerCommand>)
                {
                    BroadcastPointer(c);
                }
                else if constexpr (std::is_same_v<T, InputCommand>)
                {
                    ApplyInput(c);
                }
                }, cmd);

            localQueue.pop();
        }
    }

    void RiveRenderer::CreateRiveFile(std::span<const uint8_t> data)
    {
        _rivFile = rive::File::import(data, _renderContext.get());
        if (_rivFile)
        {
            _artboard = _rivFile->artboardDefault();
            OnArtboardChanged();
        }
    }

    void RiveRenderer::OnArtboardChanged()
    {
        if (!_artboard) return;

        // Only a state machine takes inputs, so which branch built the scene is
        // what decides whether there is an _activeStateMachine at all. It is set
        // by the branch that made the scene rather than read back off the Scene
        // pointer: StateMachineInstance, LinearAnimationInstance and StaticScene
        // are siblings under Scene, with no cast and no runtime type query
        // between them, so casting the base pointer to a state machine would
        // reinterpret whatever the file happened to contain. An artboard with
        // only an animation therefore leaves _activeStateMachine null - there is
        // nothing in it that could answer an input.
        std::unique_ptr<rive::Scene> scene;
        rive::StateMachineInstance* stateMachine = nullptr;

        const auto smCount = _artboard->stateMachineCount();
        const int defaultIdx = _artboard->defaultStateMachineIndex();

        if (defaultIdx >= 0 && defaultIdx < static_cast<int>(smCount))
        {
            auto sm = _artboard->stateMachineAt(defaultIdx);
            stateMachine = sm.get();
            scene = std::move(sm);
        }
        else if (smCount > 0)
        {
            auto sm = _artboard->stateMachineAt(0);
            stateMachine = sm.get();
            scene = std::move(sm);
        }
        else if (_artboard->animationCount() > 0)
        {
            scene = _artboard->animationAt(0);
        }
        else
        {
            scene = std::make_unique<rive::StaticScene>(_artboard.get());
        }

        if (scene)
        {
            scene->advanceAndApply(0.0f);
            _scene = std::move(scene);
            _activeStateMachine = stateMachine;
            _layoutDirty = true;
        }
    }

    void RiveRenderer::ApplyInput(InputCommand const& in)
    {
        if (!_activeStateMachine) return;

        auto inputCount = _activeStateMachine->inputCount();
        for (size_t i = 0; i < inputCount; ++i)
        {
            if (auto input = _activeStateMachine->input(i); input->name() == in.name)
            {
                switch (in.kind)
                {
                case InputCommand::Kind::Bool:
                    if (auto b = static_cast<rive::SMIBool*>(input)) b->value(in.boolValue);
                    break;
                case InputCommand::Kind::Number:
                    if (auto n = static_cast<rive::SMINumber*>(input)) n->value(in.numberValue);
                    break;
                case InputCommand::Kind::Trigger:
                    if (auto t = static_cast<rive::SMITrigger*>(input)) t->fire();
                    break;
                }
                return;
            }
        }
    }

    void RiveRenderer::BroadcastPointer(const PointerCommand& p)
    {
        if (!_scene) return;
        float x = p.x, y = p.y;
        if (TransformPoint(x, y))
        {
            rive::Vec2D pos{ x, y };
            if (p.kind == PointerCommand::Kind::Move)
            {
                _scene->pointerMove(pos);
            }
            else if (p.kind == PointerCommand::Kind::Down)
            {
                _scene->pointerDown(pos);
            }
            else if (p.kind == PointerCommand::Kind::Up)
            {
                _scene->pointerUp(pos);
            }
            else if (p.kind == PointerCommand::Kind::Exit)
            {
                // Drops hover state for every hit component and releases the
                // pointer's bookkeeping in the listener groups, so exit
                // listeners fire and a press/drag left in flight ends here.
                _scene->pointerExit(pos);
            }
        }
    }

    bool RiveRenderer::TransformPoint(float& x, float& y) const
    {
        if (!_artboard) return false;
        // Pointer positions arrive in DIPs; the view transform maps physical
        // pixels onto the artboard.
        const float scale = _rasterizationScale.load();
        auto inv = _viewTransform.invertOrIdentity();
        auto pt = inv * rive::Vec2D(x * scale, y * scale);
        x = pt.x; y = pt.y;
        return true;
    }
#pragma endregion
}
