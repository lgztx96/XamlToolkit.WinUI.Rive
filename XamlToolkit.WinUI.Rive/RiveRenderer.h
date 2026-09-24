#pragma once

#ifdef __INTELLISENSE__
#include <winrt/base.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Microsoft.Graphics.DirectX.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#endif

#ifdef WINRT_IMPORT_MODULE
// The renderer uses the composition projection directly; import it here too so
// the header is self contained no matter which translation unit pulls it in.
import winrt.Windows.Foundation.Numerics;
import winrt.Windows.Graphics;
import winrt.Microsoft.Graphics.DirectX;
import winrt.Microsoft.UI.Composition;
import winrt.Microsoft.UI.Dispatching;
import winrt.Microsoft.UI.Xaml;
import winrt.Microsoft.UI.Xaml.Hosting;
#else
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Microsoft.Graphics.DirectX.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#endif

#include <d3d11_1.h>
#include <dxgi1_2.h>
#pragma warning(push)
#pragma warning(disable: 4100 4201 4244 4245 4267 4458)
#include <rive/renderer/texture.hpp>
#include <rive/renderer/rive_renderer.hpp>
#include <rive/renderer/d3d11/render_context_d3d_impl.hpp>
#include <rive/renderer/d3d11/d3d11.hpp>
#include <rive/artboard.hpp>
#include <rive/file.hpp>
#include <rive/animation/linear_animation_instance.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/static_scene.hpp>
#pragma warning(pop)
// The interop projection for composition drawing surfaces is hand written and
// lives outside the winrt.* modules, so it is always included textually.
#include <winrt/Microsoft.UI.Composition.Interop.h>

#pragma comment(lib, "dxguid.lib")

namespace winrt::XamlToolkit::WinUI::Rive::implementation
{
    struct LoadRivCommand { std::vector<uint8_t> data; };
    struct SelectArtboardCmd { std::string name; };
    struct SelectStateMachineCmd { std::string name; };
    struct PointerCommand { float x, y; enum class Kind { Move, Down, Up, Exit } kind; };
    struct InputCommand
    {
        enum class Kind { Bool, Number, Trigger };
        std::string name;
        Kind kind;
        union { bool boolValue; float numberValue; };
    };

    using Command = std::variant<
        LoadRivCommand,
        SelectArtboardCmd,
        SelectStateMachineCmd,
        PointerCommand,
        InputCommand>;

    class RiveRenderer final : public std::enable_shared_from_this<RiveRenderer>
    {
    public:
        RiveRenderer();
        ~RiveRenderer();

        bool Attach(winrt::Microsoft::UI::Xaml::UIElement const& host);

        void UpdateSurface();

        void Detach();

        void Start();
        void Stop();
        void Pause();
        void Resume();

        void LoadFileData(std::vector<uint8_t> data);
        void SelectArtboard(std::string name);
        void SelectStateMachine(std::string name);

        void SetBoolInput(std::string name, bool value);
        void SetNumberInput(std::string name, float value);
        void FireTrigger(std::string name);

        void PointerMove(float x, float y);
        void PointerDown(float x, float y);
        void PointerUp(float x, float y);
        void PointerExit(float x, float y);

        void Enqueue(Command&& cmd);

        void ClearCommands();

    private:
        void RenderLoop(std::stop_token token);
        void ProcessCommands();
        bool TryRenderFrame(float dt);

        void CreateDeviceResources();
        void CreateCompositionResources();
        void ClearSurface();
        void EnsureFrameResources(int32_t width, int32_t height, DXGI_FORMAT format);
        void ReleaseFrameResources();
        void ReleaseDeviceResources();
        void ReleaseCompositionResources();

        void QueueReinitialize();
        void Reinitialize();

        void CreateRiveFile(std::span<const uint8_t> data);
        void OnArtboardChanged();
        void ApplyInput(const InputCommand& in);
        void BroadcastPointer(const PointerCommand& p);
        bool TransformPoint(float& x, float& y) const;

    private:
#pragma region UIThreadState
        winrt::Microsoft::UI::Xaml::UIElement _host{ nullptr };
        winrt::Microsoft::UI::Composition::ICompositionGraphicsDevice _graphicsDevice{ nullptr };
        winrt::Microsoft::UI::Composition::CompositionDrawingSurface _surface{ nullptr };
        winrt::Microsoft::UI::Composition::CompositionSurfaceBrush _surfaceBrush{ nullptr };
        winrt::Microsoft::UI::Composition::SpriteVisual _surfaceVisual{ nullptr };
        winrt::Microsoft::UI::Dispatching::DispatcherQueue _uiDispatcher{ nullptr };
        std::atomic<int> _reinitializeAttempts{ 0 };
#pragma endregion

#pragma region Shared
        std::mutex _surfaceMutex;
        winrt::com_ptr<winrt::Microsoft::UI::Composition::ICompositionDrawingSurfaceInterop> _surfaceInterop;
        int32_t _surfacePixelWidth{ 0 };
        int32_t _surfacePixelHeight{ 0 };
#pragma endregion

#pragma region RenderThreadState
        std::queue<Command> _commands;
        std::mutex _commandsMutex;
        std::unique_ptr<std::jthread> _renderThread;

        winrt::com_ptr<ID3D11Device> _device;
        winrt::com_ptr<ID3D11DeviceContext> _context;
        winrt::com_ptr<IDXGIDevice> _dxgiDevice;

        std::unique_ptr<rive::gpu::RenderContext> _renderContext;
        std::unique_ptr<rive::RiveRenderer> _renderer;
        rive::rcp<rive::gpu::RenderTargetD3D> _renderTarget;

        // Rive's own render target: the surface never sees a partial frame.
        winrt::com_ptr<ID3D11Texture2D> _frameTexture;
        int32_t _frameWidth{ 0 };
        int32_t _frameHeight{ 0 };
        DXGI_FORMAT _frameFormat{ DXGI_FORMAT_UNKNOWN };

        rive::rcp<rive::File> _rivFile;
        std::unique_ptr<rive::ArtboardInstance> _artboard;
        std::unique_ptr<rive::Scene> _scene;
        rive::StateMachineInstance* _activeStateMachine{ nullptr };

        rive::Mat2D _viewTransform{};

        std::chrono::high_resolution_clock::time_point _lastFrameTime;

        // Pointer input arrives in DIPs; physical pixels per DIP.
        std::atomic<float> _rasterizationScale{ 1.0f };
        std::atomic<bool> _layoutDirty{ true };
        std::atomic<bool> _deviceLost{ false };
        std::atomic<bool> _paused{ false };
#pragma endregion
    };
}
