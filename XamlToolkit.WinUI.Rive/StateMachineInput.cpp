#include "pch.h"
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
			if (auto rivePlayer = _rivePlayer.get())
			{
				Apply(rivePlayer, _target);
			}
		}
	}

	void StateMachineInput::SetRivePlayer(winrt::XamlToolkit::WinUI::Rive::RivePlayer const& rivePlayer)
	{
		_rivePlayer = winrt::make_weak(rivePlayer);
		Apply();
	}

	void StateMachineInput::Apply(
		[[maybe_unused]] winrt::XamlToolkit::WinUI::Rive::RivePlayer const& player, 
		[[maybe_unused]] winrt::hstring const& target)
	{

	}
}
