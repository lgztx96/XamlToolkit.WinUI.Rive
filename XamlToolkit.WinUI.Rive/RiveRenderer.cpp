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
        m_panelNative = panel;
        m_viewWidth = width;
        m_viewHeight = height;

        CreateDeviceResources();
        m_lastFrameTime = std::chrono::high_resolution_clock::now();
        return true;
    }

    void RiveRenderer::Start()
    {
        if (!m_renderThread.joinable())
        {
            m_renderThread = std::jthread(std::bind_front(&RiveRenderer::RenderLoop, this));
        }
    }

    void RiveRenderer::Stop()
    {
        if (m_renderThread.joinable())
        {
            m_renderThread.request_stop();
            m_renderThread.join();
        }
    }

    void RiveRenderer::Clear() 
    {
        std::lock_guard<std::mutex> lock(m_commandsMutex);
        while (!m_commands.empty()) 
        {
            m_commands.pop();
        }
    }

    void RiveRenderer::Pause() { m_paused = true; }
    void RiveRenderer::Resume() { m_paused = false; }

    void RiveRenderer::Resize(int w, int h) { Enqueue(ResizeCmd{ w, h }); }
    void RiveRenderer::LoadFileData(std::vector<uint8_t> data) { Enqueue(LoadFileCmd{ data }); }
    void RiveRenderer::SelectArtboard(std::string_view name) { Enqueue(SelectArtboardCmd{ std::string(name) }); }
    void RiveRenderer::SelectStateMachine(std::string_view name) { Enqueue(SelectStateMachineCmd{ std::string(name) }); }

    void RiveRenderer::SetBoolInput(std::string_view n, bool v) 
    { 
        Enqueue(InputCmd{ .name = std::string(n), .kind = InputCmd::Kind::Bool, .b = v });
    }
    void RiveRenderer::SetNumberInput(std::string_view n, float v)
    {
        Enqueue(InputCmd{ .name = std::string(n), .kind = InputCmd::Kind::Number, .f = v });
    }
    void RiveRenderer::FireTrigger(std::string_view n)
    {
        Enqueue(InputCmd{ .name = std::string(n), .kind = InputCmd::Kind::Trigger });
    }

    void RiveRenderer::PointerMove(float x, float y) { Enqueue(PointerCmd{ x, y, PointerCmd::Move }); }
    void RiveRenderer::PointerDown(float x, float y) { Enqueue(PointerCmd{ x, y, PointerCmd::Down }); }
    void RiveRenderer::PointerUp(float x, float y) { Enqueue(PointerCmd{ x, y, PointerCmd::Up }); }

    void RiveRenderer::Enqueue(Command&& cmd)
    {
        std::lock_guard<std::mutex> lock(m_commandsMutex);
        m_commands.emplace(cmd);
    }

    void RiveRenderer::RenderLoop(std::stop_token token)
    {
        while (!token.stop_requested())
        {
            ProcessCommands();

            if (!m_paused)
            {
                auto now = std::chrono::high_resolution_clock::now();
                float dt = std::chrono::duration<float>(now - m_lastFrameTime).count();
                m_lastFrameTime = now;

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
            std::lock_guard<std::mutex> lock(m_commandsMutex);
            localQueue.swap(m_commands);
        }

        while (!localQueue.empty())
        {
            auto cmd = std::move(localQueue.front());
            localQueue.pop();

            std::visit([this](auto&& c) {
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<T, ResizeCmd>)
                {
                    m_viewWidth = c.w;
                    m_viewHeight = c.h;
                    m_renderTarget.reset();
                    if (m_swapChain) m_swapChain->ResizeBuffers(2, c.w, c.h, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
                    m_layoutDirty = true;
                }
                else if constexpr (std::is_same_v<T, LoadFileCmd>)
                {
                    CreateRiveFile(c.data);
                }
                else if constexpr (std::is_same_v<T, SelectArtboardCmd>)
                {
                    if (m_file) {
                        m_artboard = m_file->artboardNamed(c.name);
                        if (m_artboard) UpdateSceneAfterArtboardChange();
                    }
                }
                else if constexpr (std::is_same_v<T, SelectStateMachineCmd>)
                {
                    if (m_artboard) {
                        auto sm = m_artboard->stateMachineNamed(c.name);
                        if (sm) {
                            m_scene = std::move(sm);
                            m_activeStateMachine = static_cast<rive::StateMachineInstance*>(m_scene.get());
                        }
                    }
                }
                else if constexpr (std::is_same_v<T, PointerCmd>)
                {
                    BroadcastPointer(c);
                }
                else if constexpr (std::is_same_v<T, InputCmd>)
                {
                    ApplyInput(c);
                }
                }, cmd);
        }
    }

    void RiveRenderer::CreateDeviceResources()
    {
        winrt::check_hresult(D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
            D3D11_SDK_VERSION, m_device.put(), nullptr, m_context.put()));

        winrt::com_ptr<IDXGIDevice> dxgiDevice;
        m_device->QueryInterface(dxgiDevice.put());
        winrt::com_ptr<IDXGIAdapter> adapter;
        dxgiDevice->GetAdapter(adapter.put());
        adapter->GetParent(IID_PPV_ARGS(m_factory.put()));

        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = m_viewWidth;
        desc.Height = m_viewHeight;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.BufferCount = 2;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        desc.SampleDesc.Count = 1;

        winrt::check_hresult(m_factory->CreateSwapChainForComposition(m_device.get(), &desc, nullptr, m_swapChain.put()));
        winrt::check_hresult(m_panelNative->SetSwapChain(m_swapChain.get()));

        m_renderContext = rive::gpu::RenderContextD3DImpl::MakeContext(m_device.get(), m_context.get(), {});
        m_renderer = std::make_unique<rive::RiveRenderer>(m_renderContext.get());
    }

    void RiveRenderer::HandleDeviceLost()
    {
        m_renderTarget.reset();
        m_renderer.reset();
        m_renderContext.reset();
        m_swapChain = nullptr;
        m_context = nullptr;
        m_device = nullptr;
        m_factory = nullptr;

        CreateDeviceResources();

        if (m_file && m_artboard)
        {
            UpdateSceneAfterArtboardChange();
            m_layoutDirty = true;
        }
    }

    bool RiveRenderer::TryRenderFrame(float dt)
    {
        if (!m_artboard || !m_scene || !m_swapChain || !m_renderContext) return true;

        if (!m_renderTarget)
        {
            m_renderTarget = m_renderContext->static_impl_cast<rive::gpu::RenderContextD3DImpl>()->makeRenderTarget(m_viewWidth, m_viewHeight);
        }

        if (m_layoutDirty)
        {
            m_viewTransform = rive::computeAlignment(rive::Fit::contain, rive::Alignment::center,
                { 0, 0, (float)m_viewWidth, (float)m_viewHeight }, m_artboard->bounds());
            m_layoutDirty = false;
        }

        winrt::com_ptr<ID3D11Texture2D> backBuffer;
        HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.put()));
        if (FAILED(hr)) return false;

        m_renderTarget->setTargetTexture(backBuffer.get());

        m_renderContext->beginFrame({ .renderTargetWidth = (uint32_t)m_viewWidth,
                                     .renderTargetHeight = (uint32_t)m_viewHeight,
                                     .clearColor = 0xff404040 });

        m_scene->advanceAndApply(dt);

        m_renderer->save();
        m_renderer->transform(m_viewTransform);
        m_artboard->draw(m_renderer.get());
        m_renderer->restore();

        m_renderContext->flush({ .renderTarget = m_renderTarget.get() });
        m_renderTarget->setTargetTexture(nullptr);

        hr = m_swapChain->Present(1, 0);
        return SUCCEEDED(hr) || hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET;
    }

    void RiveRenderer::CreateRiveFile(const std::span<uint8_t> data)
    {
        m_file = rive::File::import(data, m_renderContext.get());
        if (m_file)
        {
            m_artboard = m_file->artboardDefault() ? m_file->artboardDefault()->instance() : nullptr;
            UpdateSceneAfterArtboardChange();
        }
    }

    void RiveRenderer::UpdateSceneAfterArtboardChange()
    {
        if (!m_artboard) return;

        std::unique_ptr<rive::Scene> scene;
        auto smCount = m_artboard->stateMachineCount();
        int defaultIdx = m_artboard->defaultStateMachineIndex();

        if (defaultIdx >= 0 && defaultIdx < static_cast<int>(smCount))
            scene = m_artboard->stateMachineAt(defaultIdx);
        else if (smCount > 0)
            scene = m_artboard->stateMachineAt(0);
        else if (m_artboard->animationCount() > 0)
            scene = m_artboard->animationAt(0);
        else
            scene = std::make_unique<rive::StaticScene>(m_artboard.get());

        if (scene)
        {
            scene->advanceAndApply(0.0f);
            m_scene = std::move(scene);
            m_activeStateMachine = static_cast<rive::StateMachineInstance*>(m_scene.get());
            m_layoutDirty = true;
        }
    }

    void RiveRenderer::ApplyInput(InputCmd const& in)
    {
        auto inputCount = m_activeStateMachine->inputCount();
        for (size_t i = 0; i < inputCount; ++i)
        {
            if (auto input = m_activeStateMachine->input(i); input->name() == in.name)
            {
                switch (in.kind)
                {
                case InputCmd::Kind::Bool:
                    if (auto b = static_cast<rive::SMIBool*>(input)) b->value(in.b);
                    break;
                case InputCmd::Kind::Number:
                    if (auto n = static_cast<rive::SMINumber*>(input)) n->value(in.f);
                    break;
                case InputCmd::Kind::Trigger:
                    if (auto t = static_cast<rive::SMITrigger*>(input)) t->fire();
                    break;
                }
            }
        }
    }

    void RiveRenderer::BroadcastPointer(const PointerCmd& p)
    {
        if (!m_scene) return;
        float x = p.x, y = p.y;
        if (TransformPoint(x, y))
        {
            rive::Vec2D pos{ x, y };
            if (p.kind == PointerCmd::Move) m_scene->pointerMove(pos);
            else if (p.kind == PointerCmd::Down) m_scene->pointerDown(pos);
            else if (p.kind == PointerCmd::Up) m_scene->pointerUp(pos);
        }
    }

    bool RiveRenderer::TransformPoint(float& x, float& y) const
    {
        if (!m_artboard) return false;
        auto inv = m_viewTransform.invertOrIdentity();
        auto pt = inv * rive::Vec2D(x, y);
        x = pt.x; y = pt.y;
        return true;
    }
}