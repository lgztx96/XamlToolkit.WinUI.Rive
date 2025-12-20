#include "pch.h"
#include "StateMachineInputCollection.h"
#if __has_include("StateMachineInputCollection.g.cpp")
#include "StateMachineInputCollection.g.cpp"
#endif
#include "StateMachineInput.h"

namespace winrt::XamlToolkit::WinUI::Rive::implementation
{
    StateMachineInputCollection::StateMachineInputCollection()
    {
        VectorChanged({ this, &StateMachineInputCollection::InputsVectorChanged });
    }

    void StateMachineInputCollection::SetRivePlayer(winrt::XamlToolkit::WinUI::Rive::RivePlayer const& rivePlayer)
    {
        _rivePlayer = rivePlayer ? winrt::make_weak(rivePlayer) : nullptr;
        for (const auto& item : *this)
        {
            if (auto input = item.try_as<IStateMachineInput>())
            {
                auto inputImpl = winrt::get_self<StateMachineInput>(input);
                inputImpl->SetRivePlayer(rivePlayer);
            }
        }
    }

    void StateMachineInputCollection::InputsVectorChanged(IObservableVector<DependencyObject> const& sender, IVectorChangedEventArgs const& event)
    {
        switch (event.CollectionChange())
        {
        case CollectionChange::ItemInserted:
        case CollectionChange::ItemChanged:
        {
            auto input = sender.GetAt(event.Index()).as<IStateMachineInput>();
            auto inputImpl = winrt::get_self<StateMachineInput>(input);
            inputImpl->SetRivePlayer(_rivePlayer ? _rivePlayer.get() : nullptr);
        }
        break;
        case CollectionChange::Reset:
            for (const auto& item : sender)
            {
                if (auto input = item.try_as<IStateMachineInput>())
                {
                    auto inputImpl = winrt::get_self<StateMachineInput>(input);
                    inputImpl->SetRivePlayer(_rivePlayer ? _rivePlayer.get() : nullptr);
                }
            }
            break;
        }
    }
}
