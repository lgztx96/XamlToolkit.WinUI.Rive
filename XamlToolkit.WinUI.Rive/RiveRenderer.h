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
#include <rive/renderer/rive_renderer.hpp>
#include <rive/renderer/d3d11/render_context_d3d_impl.hpp>
#include <rive/renderer/d3d11/d3d11.hpp>
#include <rive/artboard.hpp>
#include <rive/file.hpp>
#include <rive/animation/linear_animation_instance.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/static_scene.hpp>

#include <Microsoft.UI.Xaml.Media.DxInterop.h>

namespace winrt::XamlToolkit::WinUI::Rive::implementation
{
    struct ResizeViewCommand { int width, height; };
    struct LoadRivCommand { std::vector<uint8_t> data; };
    struct SelectArtboardCmd { std::string name; };
    struct SelectStateMachineCmd { std::string name; };
    struct PointerCommand { float x, y; enum class Kind { Move, Down, Up } kind; };
    struct InputCommand
    {
        enum class Kind { Bool, Number, Trigger };
        std::string name;
        Kind kind;
        union { bool boolValue; float numberValue; };
    };

    using Command = std::variant<
        ResizeViewCommand,
        LoadRivCommand,
        SelectArtboardCmd,
        SelectStateMachineCmd,
        PointerCommand,
        InputCommand>;

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

        void ClearCommands();

    private:
        void RenderLoop(std::stop_token token);
        void ProcessCommands();
        bool TryRenderFrame(float dt);
        void CreateDeviceResources();
        void HandleDeviceLost();

        void CreateRiveFile(std::span<const uint8_t> data);
        void OnArtboardChanged();
        void ApplyInput(const InputCommand& in);
        void BroadcastPointer(const PointerCommand& p);
        bool TransformPoint(float& x, float& y) const;

    private:
        winrt::com_ptr<ISwapChainPanelNative> _panelNative{ nullptr };
        int _viewWidth{};
        int _viewHeight{};

        std::queue<Command> _commands;
        std::mutex _commandsMutex;
        std::unique_ptr<std::jthread> _renderThread;

        std::atomic<bool> _paused{ false };

        winrt::com_ptr<ID3D11Device> _device;
        winrt::com_ptr<ID3D11DeviceContext> _context;
        winrt::com_ptr<IDXGIFactory2> _factory;
        winrt::com_ptr<IDXGISwapChain1> _swapChain;

        std::unique_ptr<rive::gpu::RenderContext> _renderContext;
        std::unique_ptr<rive::RiveRenderer> _renderer;
        rive::rcp<rive::gpu::RenderTargetD3D> _renderTarget;

        rive::rcp<rive::File> _rivFile;
        std::unique_ptr<rive::ArtboardInstance> _artboard;
        std::unique_ptr<rive::Scene> _scene;
        rive::StateMachineInstance* _activeStateMachine{ nullptr };

        rive::Mat2D _viewTransform{};
        bool _layoutDirty{ true };

        std::chrono::high_resolution_clock::time_point _lastFrameTime;
    };
}