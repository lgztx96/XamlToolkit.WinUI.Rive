#pragma once

#include "StateMachineInputCollection.g.h"
#ifdef __INTELLISENSE__
#include <winrt/Windows.Foundation.Collections.h>
#endif

namespace winrt
{
    using namespace Windows::Foundation::Collections;
    using namespace Microsoft::UI::Xaml;
}

namespace winrt::XamlToolkit::WinUI::Rive::implementation
{
    struct StateMachineInputCollection : StateMachineInputCollectionT<StateMachineInputCollection>
    {
        StateMachineInputCollection();

        void SetRivePlayer(winrt::XamlToolkit::WinUI::Rive::RivePlayer const& rivePlayer);

        void InputsVectorChanged(winrt::IObservableVector<winrt::DependencyObject> const& sender, winrt::IVectorChangedEventArgs const& event);

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
