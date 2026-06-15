#pragma once

#include "RivePlayer.g.h"
#ifdef __INTELLISENSE__
#include <wil/wistd_type_traits.h>
#include <wil/cppwinrt_authoring.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/XamlToolkit.WinUI.Rive.h>
#endif
#include "RiveRenderer.h"

namespace winrt 
{
	using namespace Windows::Foundation;
	using namespace Microsoft::UI::Xaml;
	using namespace Microsoft::UI::Xaml::Controls;
}

namespace winrt::XamlToolkit::WinUI::Rive::implementation
{
	struct RivePlayer : RivePlayerT<RivePlayer>
	{
		RivePlayer();

		static const wil::single_threaded_property<winrt::DependencyProperty> SourceProperty;

		winrt::hstring Source() const;

		void Source(winrt::hstring const& value) const;

		static const wil::single_threaded_property<winrt::DependencyProperty> ArtboardProperty;

		winrt::hstring Artboard() const;

		void Artboard(winrt::hstring const& value) const;

		static const wil::single_threaded_property<winrt::DependencyProperty> StateMachineProperty;

		winrt::hstring StateMachine() const;

		void StateMachine(winrt::hstring const& value) const;

		static const wil::single_threaded_property<winrt::DependencyProperty> StateMachineInputCollectionProperty;

		winrt::XamlToolkit::WinUI::Rive::StateMachineInputCollection StateMachineInputCollection() const;

		void StateMachineInputCollection(winrt::XamlToolkit::WinUI::Rive::StateMachineInputCollection const& value) const;

		void OnApplyTemplate();

		void SetBool(winrt::hstring const& name, bool value);

		void SetNumber(winrt::hstring const& name, float value);

		void FireTrigger(winrt::hstring const& name);

	private:
		std::unique_ptr<RiveRenderer> _renderer;
		winrt::SwapChainPanel _swapChainPanel{ nullptr };
		static constexpr std::wstring_view ContainerVisualName = L"RiveSwapChain";
		int _currentSourceToken = 0;

		std::string _artboardName;
		std::string _stateMachineName;

		std::unique_ptr<std::vector<Command>> _deferredSMInputsDuringAsyncSourceLoad;

		winrt::IAsyncAction LoadSourceFileDataAsync(winrt::hstring const& uriString, int sourceToken);

		void OnLoaded(winrt::IInspectable const& s, winrt::RoutedEventArgs const& e);

		void OnUnloaded(winrt::IInspectable const& s, winrt::RoutedEventArgs const& e);

		static void OnSourceNameChanged(
			winrt::DependencyObject const& d,
			winrt::DependencyPropertyChangedEventArgs const& e);

		static void OnArtboardNameChanged(winrt::DependencyObject const& d,
			winrt::DependencyPropertyChangedEventArgs const& e);

		static void OnStateMachineNameChanged(
			winrt::DependencyObject const& d,
			winrt::DependencyPropertyChangedEventArgs const& e);

		static void OnStateMachineInputCollectionChanged(
			winrt::DependencyObject const& d,
			winrt::DependencyPropertyChangedEventArgs const& e);

		void HandleSizeChangedEvent(
			winrt::IInspectable const& sender,
			winrt::SizeChangedEventArgs const& e);

		void HandlePointerMovedEvent(
			winrt::IInspectable const& sender,
			winrt::Input::PointerRoutedEventArgs const& e);

		void HandlePointerPressedEvent(
			winrt::IInspectable const&,
			winrt::Input::PointerRoutedEventArgs const& e);

		void HandlePointerReleasedEvent(
			winrt::IInspectable const&,
			winrt::Input::PointerRoutedEventArgs const& e);
	};
}

namespace winrt::XamlToolkit::WinUI::Rive::factory_implementation
{
	struct RivePlayer : RivePlayerT<RivePlayer, implementation::RivePlayer>
	{
	};
}
