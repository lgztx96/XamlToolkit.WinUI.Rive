#include "pch.h"
#include "RiveRenderer.h"

namespace winrt::XamlToolkit::WinUI::Rive::implementation
{
    using namespace std::chrono_literals;

    RiveRenderer::RiveRenderer() = default;

    RiveRenderer::~RiveRenderer()
    {
        Stop();
    }

    bool RiveRenderer::Initialize(winrt::com_ptr<ISwapChainPanelNative> const& panel, int width, int height)
    {
        _panelNative = panel;
        _viewWidth = width;
        _viewHeight = height;

        CreateDeviceResources();
        _lastFrameTime = std::chrono::high_resolution_clock::now();
        return true;
    }

    void RiveRenderer::Start()
    {
        if (!_renderThread)
        {
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

    void RiveRenderer::ClearCommands() 
    {
        std::lock_guard<std::mutex> lock(_commandsMutex);
        while (!_commands.empty()) 
        {
            _commands.pop();
        }
    }

    void RiveRenderer::Pause() { _paused = true; }
    void RiveRenderer::Resume() { _paused = false; }

    void RiveRenderer::Resize(int width, int height) { Enqueue(ResizeViewCommand{ width, height }); }
    void RiveRenderer::LoadFileData(std::vector<uint8_t> data) { Enqueue(LoadRivCommand{ data }); }
    void RiveRenderer::SelectArtboard(std::string_view name) { Enqueue(SelectArtboardCmd{ std::string(name) }); }
    void RiveRenderer::SelectStateMachine(std::string_view name) { Enqueue(SelectStateMachineCmd{ std::string(name) }); }

    void RiveRenderer::SetBoolInput(std::string_view n, bool v) 
    { 
        Enqueue(InputCommand{ .name = std::string(n), .kind = InputCommand::Kind::Bool, .boolValue = v });
    }
    void RiveRenderer::SetNumberInput(std::string_view n, float v)
    {
        Enqueue(InputCommand{ .name = std::string(n), .kind = InputCommand::Kind::Number, .numberValue = v });
    }
    void RiveRenderer::FireTrigger(std::string_view n)
    {
        Enqueue(InputCommand{ .name = std::string(n), .kind = InputCommand::Kind::Trigger });
    }

    void RiveRenderer::PointerMove(float x, float y) { Enqueue(PointerCommand{ x, y, PointerCommand::Kind::Move }); }
    void RiveRenderer::PointerDown(float x, float y) { Enqueue(PointerCommand{ x, y, PointerCommand::Kind::Down }); }
    void RiveRenderer::PointerUp(float x, float y) { Enqueue(PointerCommand{ x, y, PointerCommand::Kind::Up }); }

    void RiveRenderer::Enqueue(Command&& cmd)
    {
        std::unique_lock<std::mutex> lock(_commandsMutex);
        _commands.emplace(cmd);
    }

    void RiveRenderer::RenderLoop(std::stop_token token)
    {
        while (!token.stop_requested())
        {
            ProcessCommands();

            if (!_paused)
            {
                auto now = std::chrono::high_resolution_clock::now();
                float dt = std::chrono::duration<float>(now - _lastFrameTime).count();
                _lastFrameTime = now;

                if (!TryRenderFrame(dt))
                {
                    HandleDeviceLost();
                }
            }

            std::this_thread::sleep_for(16ms);
        }
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
                if constexpr (std::is_same_v<T, ResizeViewCommand>)
                {
                    _viewWidth = c.width;
                    _viewHeight = c.height;
                    _renderTarget.reset();
                    if (_swapChain) _swapChain->ResizeBuffers(2, c.width, c.height, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
                    _layoutDirty = true;
                }
                else if constexpr (std::is_same_v<T, LoadRivCommand>)
                {
                    CreateRiveFile(c.data);
                }
                else if constexpr (std::is_same_v<T, SelectArtboardCmd>)
                {
                    if (_rivFile) {
                        _artboard = _rivFile->artboardNamed(c.name);
                        if (_artboard) OnArtboardChanged();
                    }
                }
                else if constexpr (std::is_same_v<T, SelectStateMachineCmd>)
                {
                    if (_artboard) {
                        if (auto sm = _artboard->stateMachineNamed(c.name)) {
                            _scene = std::move(sm);
                            _activeStateMachine = static_cast<rive::StateMachineInstance*>(_scene.get());
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

    void RiveRenderer::CreateDeviceResources()
    {
        winrt::check_hresult(D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
            D3D11_SDK_VERSION, _device.put(), nullptr, _context.put()));

        winrt::com_ptr<IDXGIDevice> dxgiDevice;
        _device->QueryInterface(dxgiDevice.put());
        winrt::com_ptr<IDXGIAdapter> adapter;
        dxgiDevice->GetAdapter(adapter.put());
        adapter->GetParent(IID_PPV_ARGS(_factory.put()));

        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = _viewWidth;
        desc.Height = _viewHeight;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.BufferCount = 2;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        desc.SampleDesc.Count = 1;

        winrt::check_hresult(_factory->CreateSwapChainForComposition(_device.get(), &desc, nullptr, _swapChain.put()));
        winrt::check_hresult(_panelNative->SetSwapChain(_swapChain.get()));

        _renderContext = rive::gpu::RenderContextD3DImpl::MakeContext(_device.get(), _context.get(), {});
        _renderer = std::make_unique<rive::RiveRenderer>(_renderContext.get());
    }

    void RiveRenderer::HandleDeviceLost()
    {
        _renderTarget.reset();
        _renderer.reset();
        _renderContext.reset();
        _swapChain = nullptr;
        _context = nullptr;
        _device = nullptr;
        _factory = nullptr;

        CreateDeviceResources();

        if (_rivFile && _artboard)
        {
            OnArtboardChanged();
            _layoutDirty = true;
        }
    }

    bool RiveRenderer::TryRenderFrame(float dt)
    {
        if (!_artboard || !_scene || !_swapChain || !_renderContext) return true;

        if (!_renderTarget)
        {
            _renderTarget = _renderContext->static_impl_cast<rive::gpu::RenderContextD3DImpl>()->makeRenderTarget(_viewWidth, _viewHeight);
        }

        if (_layoutDirty)
        {
            _viewTransform = rive::computeAlignment(
                rive::Fit::contain, 
                rive::Alignment::center,
                rive::AABB{ 0, 0, static_cast<float>(_viewWidth), static_cast<float>(_viewHeight) }, 
                _artboard->bounds());
            _layoutDirty = false;
        }

        winrt::com_ptr<ID3D11Texture2D> backBuffer;
        HRESULT hr = _swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.put()));
        if (FAILED(hr)) return false;

        _renderTarget->setTargetTexture(backBuffer.get());

        _renderContext->beginFrame({ .renderTargetWidth = static_cast<uint32_t>(_viewWidth),
                                     .renderTargetHeight = static_cast<uint32_t>(_viewHeight),
                                     .clearColor = 0x00404040 });

        _scene->advanceAndApply(dt);

        _renderer->save();
        _renderer->transform(_viewTransform);
        _artboard->draw(_renderer.get());
        _renderer->restore();

        _renderContext->flush({ .renderTarget = _renderTarget.get() });
        _renderTarget->setTargetTexture(nullptr);

        hr = _swapChain->Present(1, 0);
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
            return false;

        return SUCCEEDED(hr);
    }

    void RiveRenderer::CreateRiveFile(std::span<const uint8_t> data)
    {
        _rivFile = rive::File::import(data, _renderContext.get());
        if (_rivFile)
        {
            _artboard = _rivFile->artboardDefault() ? _rivFile->artboardDefault()->instance() : nullptr;
            OnArtboardChanged();
        }
    }

    void RiveRenderer::OnArtboardChanged()
    {
        if (!_artboard) return;

        std::unique_ptr<rive::Scene> scene;
        auto smCount = _artboard->stateMachineCount();
        int defaultIdx = _artboard->defaultStateMachineIndex();

        if (defaultIdx >= 0 && defaultIdx < static_cast<int>(smCount))
            scene = _artboard->stateMachineAt(defaultIdx);
        else if (smCount > 0)
            scene = _artboard->stateMachineAt(0);
        else if (_artboard->animationCount() > 0)
            scene = _artboard->animationAt(0);
        else
            scene = std::make_unique<rive::StaticScene>(_artboard.get());

        if (scene)
        {
            scene->advanceAndApply(0.0f);
            _scene = std::move(scene);
            _activeStateMachine = static_cast<rive::StateMachineInstance*>(_scene.get());
            _layoutDirty = true;
        }
    }

    void RiveRenderer::ApplyInput(InputCommand const& in)
    {
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
        }
    }

    bool RiveRenderer::TransformPoint(float& x, float& y) const
    {
        if (!_artboard) return false;
        auto inv = _viewTransform.invertOrIdentity();
        auto pt = inv * rive::Vec2D(x, y);
        x = pt.x; y = pt.y;
        return true;
    }
}