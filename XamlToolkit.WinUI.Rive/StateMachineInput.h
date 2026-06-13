#pragma once

#include "StateMachineInput.g.h"
#include "BoolInput.g.h"
#include "NumberInput.g.h"
#include "TriggerInput.g.h"
#ifdef __INTELLISENSE__
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <wil/wistd_type_traits.h>
#include <wil/cppwinrt_authoring.h>
#endif

namespace winrt::XamlToolkit::WinUI::Rive::implementation
{
	struct StateMachineInput : StateMachineInputT<StateMachineInput>
	{
		StateMachineInput() = default;

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
		BoolInput() = default;

		static void OnValueChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& d,
			[[maybe_unused]] winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& e)
		{
			auto input = d.as<class_type>();
			auto inputImpl = winrt::get_self<BoolInput>(input);
			inputImpl->StateMachineInput::Apply();
		}

		static inline const wil::single_threaded_property<winrt::Microsoft::UI::Xaml::DependencyProperty> ValueProperty = winrt::Microsoft::UI::Xaml::DependencyProperty::Register(
			L"Value",
			winrt::xaml_typename<winrt::Windows::Foundation::IReference<bool>>(),
			winrt::xaml_typename<class_type>(),
			winrt::Microsoft::UI::Xaml::PropertyMetadata(nullptr, &BoolInput::OnValueChanged));

		std::optional<bool> Value() const
		{
			return GetValue(ValueProperty()).try_as<bool>();
		}

		void Value(winrt::Windows::Foundation::IReference<bool> const& value) const
		{
			SetValue(ValueProperty(), value);
		}

		void Apply(winrt::XamlToolkit::WinUI::Rive::RivePlayer const& rivePlayer, winrt::hstring const& inputName) override
		{
			if (auto boolean = Value())
			{
				rivePlayer.SetBool(inputName, *boolean);
			}
		}
	};

	struct NumberInput : NumberInputT<NumberInput, StateMachineInput>
	{
		NumberInput() = default;

		static void OnValueChanged(
			winrt::Microsoft::UI::Xaml::DependencyObject const& d,
			[[maybe_unused]] winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& e)
		{
			auto input = d.as<class_type>();
			auto inputImpl = winrt::get_self<NumberInput>(input);
			inputImpl->StateMachineInput::Apply();
		}

		static inline const wil::single_threaded_property<winrt::Microsoft::UI::Xaml::DependencyProperty> ValueProperty = winrt::Microsoft::UI::Xaml::DependencyProperty::Register(
			L"Value",
			winrt::xaml_typename<winrt::Windows::Foundation::IReference<double>>(),
			winrt::xaml_typename<class_type>(),
			winrt::Microsoft::UI::Xaml::PropertyMetadata(nullptr, &NumberInput::OnValueChanged));

		std::optional<double> Value() const
		{
			return GetValue(ValueProperty()).try_as<double>();
		}

		void Value(winrt::Windows::Foundation::IReference<double> const& value) const
		{
			SetValue(ValueProperty(), value);
		}

		void Apply(winrt::XamlToolkit::WinUI::Rive::RivePlayer const& rivePlayer, winrt::hstring const& inputName) override
		{
			if (auto number = Value())
			{
				rivePlayer.SetNumber(inputName, static_cast<float>(*number));
			}
		}
	};

	struct TriggerInput : TriggerInputT<TriggerInput, StateMachineInput>
	{
		TriggerInput() = default;

		void Fire()
		{
			if (auto target = Target(); !target.empty())
			{
				if (auto rivePlayer = _rivePlayer.get())
				{
					rivePlayer.FireTrigger(target);
				}
			}
		};
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
