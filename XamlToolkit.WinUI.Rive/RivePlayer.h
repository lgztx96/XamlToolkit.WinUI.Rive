#pragma once

#include "RivePlayer.g.h"
#ifdef __INTELLISENSE__
#include <wil/wistd_type_traits.h>
#include <wil/cppwinrt_authoring.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Input.h>
#endif
#include "RiveRenderer.h"

namespace winrt::XamlToolkit::WinUI::Rive::implementation
{
	struct RivePlayer : RivePlayerT<RivePlayer>
	{
		RivePlayer();

		static const wil::single_threaded_property<winrt::Microsoft::UI::Xaml::DependencyProperty> SourceProperty;

		winrt::hstring Source() const;

		void Source(winrt::hstring const& value) const;

		static const wil::single_threaded_property<winrt::Microsoft::UI::Xaml::DependencyProperty> ArtboardProperty;

		winrt::hstring Artboard() const;

		void Artboard(winrt::hstring const& value) const;

		static const wil::single_threaded_property<winrt::Microsoft::UI::Xaml::DependencyProperty> StateMachineProperty;

		winrt::hstring StateMachine() const;

		void StateMachine(winrt::hstring const& value) const;

		static const wil::single_threaded_property<winrt::Microsoft::UI::Xaml::DependencyProperty> StateMachineInputCollectionProperty;

		winrt::XamlToolkit::WinUI::Rive::StateMachineInputCollection StateMachineInputCollection() const;

		void StateMachineInputCollection(winrt::XamlToolkit::WinUI::Rive::StateMachineInputCollection const& value) const;

		void OnApplyTemplate();

		void SetBool(winrt::hstring const& name, bool value);

		void SetNumber(winrt::hstring const& name, float value);

		void FireTrigger(winrt::hstring const& name);

	private:
		std::unique_ptr<RiveRenderer> _renderer;
		winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel _swapChainPanel{ nullptr };
		static constexpr std::wstring_view ContainerVisualName = L"RiveSwapChain";
		int _currentSourceToken = 0;

		std::string _artboardName;
		std::string _stateMachineName;

		std::unique_ptr<std::vector<Command>> _deferredSMInputsDuringAsyncSourceLoad;

		winrt::Windows::Foundation::IAsyncAction LoadSourceFileDataAsync(winrt::hstring const& uriString, int sourceToken);

		void OnLoaded(winrt::Windows::Foundation::IInspectable const& s, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

		void OnUnloaded(winrt::Windows::Foundation::IInspectable const& s, winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e);

		static void OnSourceNameChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& d,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& e);

		static void OnArtboardNameChanged(winrt::Microsoft::UI::Xaml::DependencyObject const& d,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& e);

		static void OnStateMachineNameChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& d,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& e);

		static void OnStateMachineInputCollectionChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& d,
			winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& e);

		void HandleSizeChangedEvent(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& e);

		void HandlePointerMovedEvent(
			winrt::Windows::Foundation::IInspectable const& sender,
			winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);

		void HandlePointerPressedEvent(
			winrt::Windows::Foundation::IInspectable const&,
			winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);

		void HandlePointerReleasedEvent(
			winrt::Windows::Foundation::IInspectable const&,
			winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e);
	};
}

namespace winrt::XamlToolkit::WinUI::Rive::factory_implementation
{
	struct RivePlayer : RivePlayerT<RivePlayer, implementation::RivePlayer>
	{
	};
}
