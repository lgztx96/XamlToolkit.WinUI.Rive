#include "pch.h"
#include "winrt_module_imports.h"
#include "StateMachineInput.h"
#if __has_include("StateMachineInput.g.cpp")
#include "StateMachineInput.g.cpp"
#endif
#if __has_include("BoolInput.g.cpp")
#include "BoolInput.g.cpp"
#endif
#if __has_include("NumberInput.g.cpp")
#include "NumberInput.g.cpp"
#endif
#if __has_include("TriggerInput.g.cpp")
#include "TriggerInput.g.cpp"
#endif

namespace winrt::XamlToolkit::WinUI::Rive::implementation
{
#pragma region StateMachineInput
	winrt::hstring StateMachineInput::Target() const { return _target; }

	void StateMachineInput::Target(winrt::hstring const& value)
	{
		_target = value;
		Apply();
	}

	void StateMachineInput::Apply()
	{
		if (!_target.empty())
		{
			if (const auto rivePlayer = _rivePlayer.get())
			{
				Apply(rivePlayer, _target);
			}
		}
	}

	void StateMachineInput::SetRivePlayer(winrt::XamlToolkit::WinUI::Rive::RivePlayer const& rivePlayer)
	{
		_rivePlayer = rivePlayer;
		Apply();
	}

	void StateMachineInput::Apply(
		[[maybe_unused]] winrt::XamlToolkit::WinUI::Rive::RivePlayer const& player, 
		[[maybe_unused]] winrt::hstring const& target)
	{

	}
#pragma endregion

#pragma region BoolInput
	void BoolInput::OnValueChanged(
		winrt::DependencyObject const& d,
		[[maybe_unused]] winrt::DependencyPropertyChangedEventArgs const& e)
	{
		const auto input = d.as<class_type>();
		const auto inputImpl = winrt::get_self<BoolInput>(input);
		inputImpl->StateMachineInput::Apply();
	}

	const wil::single_threaded_property<winrt::DependencyProperty> BoolInput::ValueProperty =
		winrt::DependencyProperty::Register(
			L"Value",
			winrt::xaml_typename<winrt::IReference<bool>>(),
			winrt::xaml_typename<class_type>(),
			winrt::PropertyMetadata(nullptr, &BoolInput::OnValueChanged));

	std::optional<bool> BoolInput::Value() const
	{
		return GetValue(ValueProperty()).try_as<bool>();
	}

	void BoolInput::Value(winrt::IReference<bool> const& value) const
	{
		SetValue(ValueProperty(), value);
	}

	void BoolInput::Apply(winrt::XamlToolkit::WinUI::Rive::RivePlayer const& rivePlayer, winrt::hstring const& inputName)
	{
		if (const auto boolean = Value())
		{
			rivePlayer.SetBool(inputName, *boolean);
		}
	}
#pragma endregion

#pragma region NumberInput
	void NumberInput::OnValueChanged(
		winrt::DependencyObject const& d,
		[[maybe_unused]] winrt::DependencyPropertyChangedEventArgs const& e)
	{
		const auto input = d.as<class_type>();
		const auto inputImpl = winrt::get_self<NumberInput>(input);
		inputImpl->StateMachineInput::Apply();
	}

	const wil::single_threaded_property<winrt::DependencyProperty> NumberInput::ValueProperty =
		winrt::DependencyProperty::Register(
			L"Value",
			winrt::xaml_typename<winrt::IReference<double>>(),
			winrt::xaml_typename<class_type>(),
			winrt::PropertyMetadata(nullptr, &NumberInput::OnValueChanged));

	std::optional<double> NumberInput::Value() const
	{
		return GetValue(ValueProperty()).try_as<double>();
	}

	void NumberInput::Value(winrt::IReference<double> const& value) const
	{
		SetValue(ValueProperty(), value);
	}

	void NumberInput::Apply(winrt::XamlToolkit::WinUI::Rive::RivePlayer const& rivePlayer, winrt::hstring const& inputName)
	{
		if (const auto number = Value())
		{
			rivePlayer.SetNumber(inputName, static_cast<float>(*number));
		}
	}
#pragma endregion

#pragma region TriggerInput
	void TriggerInput::Fire()
	{
		if (const auto target = Target(); !target.empty())
		{
			if (const auto rivePlayer = _rivePlayer.get())
			{
				rivePlayer.FireTrigger(target);
			}
		}
	};
#pragma endregion
}
