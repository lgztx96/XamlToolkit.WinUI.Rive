#pragma once

#include "StateMachineInput.g.h"
#include "BoolInput.g.h"
#include "NumberInput.g.h"
#include "TriggerInput.g.h"
#ifdef __INTELLISENSE__
#include <winrt/Windows.Foundation.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <wil/wistd_type_traits.h>
#include <wil/cppwinrt_authoring.h>
#endif

namespace winrt
{
	using namespace ::winrt::Windows::Foundation;
	using namespace ::winrt::Windows::UI::Xaml::Interop;
	using namespace ::winrt::Microsoft::UI::Xaml;
}

namespace winrt::XamlToolkit::WinUI::Rive::implementation
{
	struct StateMachineInput : StateMachineInputT<StateMachineInput>
	{
		winrt::hstring Target() const;

		void Target(winrt::hstring const& value);

		void Apply();

		void SetRivePlayer(winrt::XamlToolkit::WinUI::Rive::RivePlayer const& rivePlayer);

		virtual void Apply(winrt::XamlToolkit::WinUI::Rive::RivePlayer const& player, winrt::hstring const& target);

	protected:
		winrt::hstring _target;
		winrt::weak_ref<winrt::XamlToolkit::WinUI::Rive::RivePlayer> _rivePlayer;
	};

	struct BoolInput : BoolInputT<BoolInput, StateMachineInput>
	{
		static void OnValueChanged(
			winrt::DependencyObject const& d,
			[[maybe_unused]] winrt::DependencyPropertyChangedEventArgs const& e);

		static const wil::single_threaded_property<winrt::DependencyProperty> ValueProperty;

		std::optional<bool> Value() const;

		void Value(winrt::IReference<bool> const& value) const;

		void Apply(winrt::XamlToolkit::WinUI::Rive::RivePlayer const& rivePlayer, winrt::hstring const& inputName) override;
	};

	struct NumberInput : NumberInputT<NumberInput, StateMachineInput>
	{
		static void OnValueChanged(
			winrt::DependencyObject const& d,
			[[maybe_unused]] winrt::DependencyPropertyChangedEventArgs const& e);

		static const wil::single_threaded_property<winrt::DependencyProperty> ValueProperty;

		std::optional<double> Value() const;

		void Value(winrt::IReference<double> const& value) const;

		void Apply(winrt::XamlToolkit::WinUI::Rive::RivePlayer const& rivePlayer, winrt::hstring const& inputName) override;
	};

	struct TriggerInput : TriggerInputT<TriggerInput, StateMachineInput>
	{
		void Fire();
	};
}

namespace winrt::XamlToolkit::WinUI::Rive::factory_implementation
{
	struct StateMachineInput : StateMachineInputT<StateMachineInput, implementation::StateMachineInput>
	{
	};

	struct BoolInput : BoolInputT<BoolInput, implementation::BoolInput>
	{
	};

	struct NumberInput : NumberInputT<NumberInput, implementation::NumberInput>
	{
	};

	struct TriggerInput : TriggerInputT<TriggerInput, implementation::TriggerInput>
	{
	};
}
