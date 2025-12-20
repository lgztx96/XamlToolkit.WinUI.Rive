#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winrt/base.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <queue>
#include <variant>
#include <string>
#include <string_view>

#include <rive/renderer/render_context.hpp>
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/d3d11/render_context_d3d_impl.hpp"
#include "rive/renderer/d3d11/d3d11.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/static_scene.hpp"

#include <Microsoft.UI.Xaml.Media.DxInterop.h>

namespace winrt::XamlToolkit::WinUI::Rive::implementation
{
    struct ResizeCmd { int w, h; };
    struct LoadFileCmd { std::vector<uint8_t> data; };
    struct SelectArtboardCmd { std::string name; };
    struct SelectStateMachineCmd { std::string name; };
    struct PointerCmd { float x, y; enum Kind { Move, Down, Up } kind; };
    struct InputCmd
    {
        enum class Kind { Bool, Number, Trigger };
        std::string name;
        Kind kind;
        union { bool b; float f; };
    };

    using Command = std::variant<
        ResizeCmd,
        LoadFileCmd,
        SelectArtboardCmd,
        SelectStateMachineCmd,
        PointerCmd,
        InputCmd>;

    class RiveRenderer final
    {
    public:
        RiveRenderer();
        ~RiveRenderer();

        bool Initialize(winrt::com_ptr<ISwapChainPanelNative> const& panel, int width, int height);
        void Start();
        void Stop();
        void Pause();
        void Resume();

        void Resize(int width, int height);
        void LoadFileData(std::vector<uint8_t> data);
        void SelectArtboard(std::string_view name);
        void SelectStateMachine(std::string_view name);

        void SetBoolInput(std::string_view name, bool value);
        void SetNumberInput(std::string_view name, float value);
        void FireTrigger(std::string_view name);

        void PointerMove(float x, float y);
        void PointerDown(float x, float y);
        void PointerUp(float x, float y);

        void Enqueue(Command&& cmd);

        void Clear();

    private:
        // ---------- Rendering ----------
        void RenderLoop(std::stop_token token);
        void ProcessCommands();
        bool TryRenderFrame(float dt);
        void CreateDeviceResources();
        void HandleDeviceLost();

        // ---------- Rive Operations (run on render thread) ----------
        void CreateRiveFile(const std::span<uint8_t> data);
        void UpdateSceneAfterArtboardChange();
        void ApplyInput(const InputCmd& in);
        void BroadcastPointer(const PointerCmd& p);
        bool TransformPoint(float& x, float& y) const;

    private:
        winrt::com_ptr<ISwapChainPanelNative> m_panelNative{ nullptr };
        int m_viewWidth{};
        int m_viewHeight{};

        std::queue<Command> m_commands;
        std::mutex m_commandsMutex;
        std::jthread m_renderThread;

        std::atomic<bool> m_paused{ false };

        winrt::com_ptr<ID3D11Device> m_device;
        winrt::com_ptr<ID3D11DeviceContext> m_context;
        winrt::com_ptr<IDXGIFactory2> m_factory;
        winrt::com_ptr<IDXGISwapChain1> m_swapChain;

        std::unique_ptr<rive::gpu::RenderContext> m_renderContext;
        std::unique_ptr<rive::RiveRenderer> m_renderer;
        rive::rcp<rive::gpu::RenderTargetD3D> m_renderTarget;

        rive::rcp<rive::File> m_file;
        std::unique_ptr<rive::ArtboardInstance> m_artboard;
        std::unique_ptr<rive::Scene> m_scene;
        rive::StateMachineInstance* m_activeStateMachine{ nullptr };

        rive::Mat2D m_viewTransform{};
        bool m_layoutDirty{ true };

        std::chrono::high_resolution_clock::time_point m_lastFrameTime;
    };
}