#pragma once

#include "StateMachineInputCollection.g.h"
#ifdef __INTELLISENSE__
#include <winrt/Windows.Foundation.Collections.h>
#endif

namespace winrt::XamlToolkit::WinUI::Rive::implementation
{
    using namespace winrt::Windows::Foundation::Collections;
    using namespace winrt::Microsoft::UI::Xaml;

    struct StateMachineInputCollection : StateMachineInputCollectionT<StateMachineInputCollection>
    {
        StateMachineInputCollection();

        void SetRivePlayer(winrt::XamlToolkit::WinUI::Rive::RivePlayer const& rivePlayer);

        void InputsVectorChanged(IObservableVector<DependencyObject> const& sender, IVectorChangedEventArgs const& event);

    private:
        winrt::weak_ref<winrt::XamlToolkit::WinUI::Rive::RivePlayer> _rivePlayer;
    };
}

namespace winrt::XamlToolkit::WinUI::Rive::factory_implementation
{
    struct StateMachineInputCollection : StateMachineInputCollectionT<StateMachineInputCollection, implementation::StateMachineInputCollection>
    {
    };
}
