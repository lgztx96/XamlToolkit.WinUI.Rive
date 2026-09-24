#include "pch.h"
#include "winrt_module_imports.h"
#include "RivePlayer.h"
#if __has_include("RivePlayer.g.cpp")
#include "RivePlayer.g.cpp"
#endif
#ifdef __INTELLISENSE__
#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Input.h>
#include <filesystem>
#include <fstream>
#endif

#include "StateMachineInputCollection.h"

namespace winrt
{
	using namespace ::winrt::Windows::Foundation;
	using namespace ::winrt::Windows::Storage;
	using namespace ::winrt::Windows::Web::Http;
	using namespace ::winrt::Windows::UI::Xaml::Interop;
	using namespace ::winrt::Microsoft::UI::Xaml;
	using namespace ::winrt::Microsoft::UI::Xaml::Controls;
}

namespace winrt::XamlToolkit::WinUI::Rive::implementation
{
	const wil::single_threaded_property<winrt::DependencyProperty> RivePlayer::SourceProperty = 
		winrt::DependencyProperty::Register(
			L"Source",
			winrt::xaml_typename<winrt::hstring>(),
			winrt::xaml_typename<class_type>(),
			winrt::PropertyMetadata(winrt::box_value(L""), &RivePlayer::OnSourceNameChanged));

	winrt::hstring RivePlayer::Source() const
	{
		return winrt::unbox_value<winrt::hstring>(GetValue(SourceProperty()));
	}

	void RivePlayer::Source(winrt::hstring const& value) const
	{
		SetValue(SourceProperty(), winrt::box_value(value));
	}

	const wil::single_threaded_property<winrt::DependencyProperty> RivePlayer::ArtboardProperty = 
		winrt::DependencyProperty::Register(
			L"Artboard",
			winrt::xaml_typename<winrt::hstring>(),
			winrt::xaml_typename<class_type>(),
			winrt::PropertyMetadata(winrt::box_value(L""), &RivePlayer::OnArtboardNameChanged));

	winrt::hstring RivePlayer::Artboard() const
	{
		return winrt::unbox_value<winrt::hstring>(GetValue(ArtboardProperty()));
	}

	void RivePlayer::Artboard(winrt::hstring const& value) const
	{
		SetValue(ArtboardProperty(), winrt::box_value(value));
	}

	const wil::single_threaded_property<winrt::DependencyProperty> RivePlayer::StateMachineProperty = 
		winrt::DependencyProperty::Register(
			L"StateMachine",
			winrt::xaml_typename<winrt::hstring>(),
			winrt::xaml_typename<class_type>(),
			winrt::PropertyMetadata(winrt::box_value(L""), &RivePlayer::OnStateMachineNameChanged));

	winrt::hstring RivePlayer::StateMachine() const
	{
		return winrt::unbox_value<winrt::hstring>(GetValue(StateMachineProperty()));
	}

	void RivePlayer::StateMachine(winrt::hstring const& value) const
	{
		SetValue(StateMachineProperty(), winrt::box_value(value));
	}

	const wil::single_threaded_property<winrt::DependencyProperty> RivePlayer::StateMachineInputCollectionProperty = 
		winrt::DependencyProperty::Register(
			L"StateMachineInputCollection",
			winrt::xaml_typename<winrt::XamlToolkit::WinUI::Rive::StateMachineInputCollection>(),
			winrt::xaml_typename<class_type>(),
			winrt::PropertyMetadata(nullptr, &RivePlayer::OnStateMachineInputCollectionChanged));

	winrt::XamlToolkit::WinUI::Rive::StateMachineInputCollection RivePlayer::StateMachineInputCollection() const
	{
		return GetValue(StateMachineInputCollectionProperty()).try_as<winrt::XamlToolkit::WinUI::Rive::StateMachineInputCollection>();
	}

	void RivePlayer::StateMachineInputCollection(winrt::XamlToolkit::WinUI::Rive::StateMachineInputCollection const& value) const
	{
		SetValue(StateMachineInputCollectionProperty(), value);
	}

	RivePlayer::RivePlayer()
	{
		DefaultStyleKey(winrt::box_value(winrt::xaml_typename<class_type>()));
		Loaded({ this, &RivePlayer::OnLoaded });
		Unloaded({ this, &RivePlayer::OnUnloaded });
		SizeChanged({ this, &RivePlayer::HandleSizeChangedEvent });
		PointerMoved({ this, &RivePlayer::HandlePointerMovedEvent });
		PointerEntered({ this, &RivePlayer::HandlePointerEnteredEvent });
		PointerExited({ this, &RivePlayer::HandlePointerExitedEvent });
		PointerPressed({ this, &RivePlayer::HandlePointerPressedEvent });
		PointerReleased({ this, &RivePlayer::HandlePointerReleasedEvent });
		PointerCaptureLost({ this, &RivePlayer::HandlePointerCaptureLostEvent });
		PointerCanceled({ this, &RivePlayer::HandlePointerCanceledEvent });
		_renderer = std::make_unique<RiveRenderer>();

		StateMachineInputCollection(winrt::make<implementation::StateMachineInputCollection>());
	}

	void RivePlayer::OnApplyTemplate()
	{
		_hostElement = GetTemplateChild(ContainerVisualName).try_as<winrt::UIElement>();
	}

	void RivePlayer::SetBool(winrt::hstring const& name, bool value)
	{
		std::string utf8Name = winrt::to_string(name);
		if (_deferredSMInputsDuringAsyncSourceLoad)
		{
			// A source file is currently loading async. Don't set this input until it completes.
			_deferredSMInputsDuringAsyncSourceLoad->emplace_back(
				InputCommand{ .name = std::move(utf8Name), .kind = InputCommand::Kind::Bool, .boolValue = value });
		}
		else
		{
			_renderer->SetBoolInput(std::move(utf8Name), value);
		}
	}

	void RivePlayer::SetNumber(winrt::hstring const& name, float value)
	{
		std::string utf8Name = winrt::to_string(name);
		if (_deferredSMInputsDuringAsyncSourceLoad)
		{
			// A source file is currently loading async. Don't set this input until it completes.
			_deferredSMInputsDuringAsyncSourceLoad->emplace_back(
				InputCommand{ .name = std::move(utf8Name), .kind = InputCommand::Kind::Number, .numberValue = value });
		}
		else
		{
			_renderer->SetNumberInput(std::move(utf8Name), value);
		}
	}

	void RivePlayer::FireTrigger(winrt::hstring const& name)
	{
		std::string utf8Name = winrt::to_string(name);
		if (_deferredSMInputsDuringAsyncSourceLoad)
		{
			// A source file is currently loading async. Don't set this input until it completes.
			_deferredSMInputsDuringAsyncSourceLoad->emplace_back(
				InputCommand{ .name = std::move(utf8Name), .kind = InputCommand::Kind::Trigger });
		}
		else
		{
			_renderer->FireTrigger(std::move(utf8Name));
		}
	}

	void RivePlayer::OnLoaded(
		[[maybe_unused]] winrt::IInspectable const& s,
		[[maybe_unused]] winrt::RoutedEventArgs const& e)
	{
		if (!_hostElement)
		{
			return;
		}

		_renderer->Attach(_hostElement);

		// The drawing surface is sized in physical pixels, so a change of the
		// rasterization scale (a window moved to another display) needs a resize
		// just like a size change does.
		if (!_xamlRootSubscribed)
		{
			if (const auto xamlRoot = _hostElement.XamlRoot())
			{
				_xamlRoot = xamlRoot;
				_xamlRootChangedToken = xamlRoot.Changed({ this, &RivePlayer::HandleXamlRootChangedEvent });
				_xamlRootSubscribed = true;
			}
		}

		_renderer->Start();
	}

	void RivePlayer::OnUnloaded(
		[[maybe_unused]] winrt::IInspectable const& s,
		[[maybe_unused]] winrt::RoutedEventArgs const& e)
	{
		_renderer->Stop();
		_renderer->Detach();

		if (_xamlRootSubscribed)
		{
			_xamlRoot.Changed(_xamlRootChangedToken);
			_xamlRoot = nullptr;
			_xamlRootChangedToken = {};
			_xamlRootSubscribed = false;
		}
	}

	void RivePlayer::OnSourceNameChanged(
		winrt::DependencyObject const& d,
		winrt::DependencyPropertyChangedEventArgs const& e)
	{
		const auto player = d.try_as<class_type>();
		const auto playerImpl = winrt::get_self<RivePlayer>(player);
		const auto newSourceName = winrt::unbox_value<winrt::hstring>(e.NewValue());
		// Clear the current Scene while we wait for the new one to load.
		playerImpl->_renderer->ClearCommands();
		++playerImpl->_currentSourceToken;  // Cancel any other active async source load operation.
		// Defer state machine inputs here until the new file is loaded.
		playerImpl->_deferredSMInputsDuringAsyncSourceLoad = std::make_unique<std::vector<Command>>();
		playerImpl->LoadSourceFileDataAsync(newSourceName, playerImpl->_currentSourceToken);
	}

	void RivePlayer::OnArtboardNameChanged(winrt::DependencyObject const& d,
		winrt::DependencyPropertyChangedEventArgs const& e)
	{
		const auto player = d.try_as<class_type>();
		const auto playerImpl = winrt::get_self<RivePlayer>(player);
		const auto newArtboardName = winrt::unbox_value<winrt::hstring>(e.NewValue());
		const auto utf8ArtboardName = winrt::to_string(newArtboardName);
		playerImpl->_artboardName = utf8ArtboardName;
		if (playerImpl->_deferredSMInputsDuringAsyncSourceLoad)
		{
			// If a file is currently loading async, it will apply the new artboard once
			// it completes. Loading a new artboard also invalidates any state machine
			// inputs that were waiting for the file load.
			playerImpl->_deferredSMInputsDuringAsyncSourceLoad->clear();
		}
		else
		{
			playerImpl->_renderer->SelectArtboard(std::move(utf8ArtboardName));
		}
	}

	void RivePlayer::OnStateMachineNameChanged(
		winrt::DependencyObject const& d,
		winrt::DependencyPropertyChangedEventArgs const& e)
	{
		const auto player = d.try_as<class_type>();
		const auto playerImpl = winrt::get_self<RivePlayer>(player);
		const auto newStateMachineName = winrt::unbox_value<winrt::hstring>(e.NewValue());
		const auto utf8StateMachineName = winrt::to_string(newStateMachineName);
		playerImpl->_stateMachineName = utf8StateMachineName;
		if (playerImpl->_deferredSMInputsDuringAsyncSourceLoad)
		{
			// If a file is currently loading async, it will apply the new state machine
			// once it completes. Loading a new state machine also invalidates any state
			// machine inputs that were waiting for the file load.
			playerImpl->_deferredSMInputsDuringAsyncSourceLoad->clear();
		}
		else
		{
			playerImpl->_renderer->SelectStateMachine(std::move(utf8StateMachineName));
		}
	}

	void RivePlayer::OnStateMachineInputCollectionChanged(
		winrt::DependencyObject const& d,
		winrt::DependencyPropertyChangedEventArgs const& e)
	{
		// Clear the RivePlayer on the old reference so it quits updating us.
		if (const auto oldCollection = e.OldValue().try_as<winrt::XamlToolkit::WinUI::Rive::StateMachineInputCollection>())
		{
			const auto oldCollectionImpl = winrt::get_self<implementation::StateMachineInputCollection>(oldCollection);
			oldCollectionImpl->SetRivePlayer(nullptr);
		}

		if (const auto newCollection = e.NewValue().try_as<winrt::XamlToolkit::WinUI::Rive::StateMachineInputCollection>())
		{
			const auto newCollectionImpl = winrt::get_self<implementation::StateMachineInputCollection>(newCollection);
			newCollectionImpl->SetRivePlayer(d.try_as<class_type>()); 
		}
	}

	static winrt::Uri TryCreate(winrt::hstring const& uriString)
	{
		try
		{
			winrt::Uri uri(uriString);
			return uri;
		}
		catch ([[maybe_unused]] winrt::hresult_error const& ex)
		{
			return nullptr;
		}
	}

	winrt::IAsyncAction RivePlayer::LoadSourceFileDataAsync(winrt::hstring const& uriString, int sourceToken)
	{
		winrt::Uri uri = TryCreate(uriString);
		if (uri == nullptr)
		{
			co_return;
		}

		std::vector<uint8_t> data;
		const auto scheme = uri.SchemeName();
		if (scheme == L"http" || scheme == L"https")
		{
			try
			{
				winrt::HttpClient httpClient;
				winrt::HttpResponseMessage response = co_await httpClient.GetAsync(uri);
				if (response.IsSuccessStatusCode())
				{
					const auto& buffer = co_await response.Content().ReadAsBufferAsync();
					data.resize(buffer.Length());
					std::copy(buffer.data(), buffer.data() + buffer.Length(), data.data());
				}
			}
			catch ([[maybe_unused]] winrt::hresult_error const& ex)
			{
				// TODO: Load a 404 file?
			}
		}
		else if (scheme == L"ms-appx")
		{
			const auto& file = co_await winrt::StorageFile::GetFileFromApplicationUriAsync(uri);
			if (file && sourceToken == _currentSourceToken)
			{
				const auto& buffer = co_await winrt::FileIO::ReadBufferAsync(file);
				data.resize(buffer.Length());
				std::copy(buffer.data(), buffer.data() + buffer.Length(), data.data());
			}
		}
		else if (scheme == L"file")
		{
			const auto path = winrt::to_string(uriString);
			if (auto fs = std::ifstream(path, std::ios::binary | std::ios::ate))
			{
				const auto size = fs.tellg();
				data.resize(static_cast<size_t>(size));
				fs.seekg(0);
				fs.read(reinterpret_cast<char*>(data.data()), size);
			}
		}

		if (!data.empty() && sourceToken == _currentSourceToken)
		{
			_renderer->LoadFileData(std::move(data));
			// Apply deferred state machine inputs once the scene is fully loaded.
			for (auto& stateMachineInput : *_deferredSMInputsDuringAsyncSourceLoad)
			{
				_renderer->Enqueue(std::move(stateMachineInput));
			}
		}

		_deferredSMInputsDuringAsyncSourceLoad.reset();
	}

	void RivePlayer::HandleSizeChangedEvent(
		[[maybe_unused]] winrt::IInspectable const& sender,
		[[maybe_unused]] winrt::SizeChangedEventArgs const& e)
	{
		if (_renderer)
		{
			_renderer->UpdateSurface();
		}
	}

	void RivePlayer::HandleXamlRootChangedEvent(
		[[maybe_unused]] winrt::XamlRoot const& sender,
		[[maybe_unused]] winrt::XamlRootChangedEventArgs const& args)
	{
		if (_renderer)
		{
			_renderer->UpdateSurface();
		}
	}

	void RivePlayer::HandlePointerMovedEvent(
		winrt::IInspectable const& sender,
		winrt::Input::PointerRoutedEventArgs const& e)
	{
		const auto uiElement = sender.as<winrt::UIElement>();
		const auto pointerPos = e.GetCurrentPoint(uiElement).Position();
		_renderer->PointerMove(pointerPos.X, pointerPos.Y);
	}

	void RivePlayer::HandlePointerEnteredEvent(
		winrt::IInspectable const& sender,
		winrt::Input::PointerRoutedEventArgs const& e)
	{
		// Entered can arrive with no preceding Moved (a control appearing
		// under a resting cursor, or a pointer that jumped in), and the
		// runtime only learns about hover from a move/down/up, so forward it
		// as a move to engage enter listeners right away.
		//
		// Touch is skipped: it has no hover, and its Entered arrives together
		// with the touch down, so a synthetic move would engage hover listeners
		// for a finger that is really pressing. Pressed/Moved/Released still
		// reach the runtime, and leaving Exited unfiltered cannot strand a hover
		// that was never entered - an exit for a hover that does not exist only
		// clears state that is already clear.
		using winrt::Microsoft::UI::Input::PointerDeviceType;
		const auto deviceType = e.Pointer().PointerDeviceType();
		if (deviceType != PointerDeviceType::Mouse && deviceType != PointerDeviceType::Pen)
		{
			return;
		}

		const auto uiElement = sender.as<winrt::UIElement>();
		const auto pointerPos = e.GetCurrentPoint(uiElement).Position();
		_renderer->PointerMove(pointerPos.X, pointerPos.Y);
	}

	void RivePlayer::HandlePointerExitedEvent(
		winrt::IInspectable const& sender,
		winrt::PointerRoutedEventArgs const& e)
	{
		const auto uiElement = sender.as<winrt::UIElement>();
		const auto pointerPos = e.GetCurrentPoint(uiElement).Position();
		_renderer->PointerExit(pointerPos.X, pointerPos.Y);
	}

	void RivePlayer::HandlePointerPressedEvent(
		winrt::IInspectable const& sender,
		winrt::PointerRoutedEventArgs const& e)
	{
		const auto uiElement = sender.as<winrt::UIElement>();
		const auto pointerPos = e.GetCurrentPoint(uiElement).Position();
		_pointerDown = true;
		_renderer->PointerDown(pointerPos.X, pointerPos.Y);

		// Hold the pointer for the duration of the drag so it keeps reporting
		// positions past the control's bounds instead of stopping at the edge.
		// Touch is deliberately left uncaptured: an outer ScrollViewer pans on a
		// finger drag, and capturing would swallow that gesture and make the
		// control un-scrollable by touch. A finger still reaches the runtime
		// through Pressed/Moved/Released, it just stops being reported once it
		// leaves the control.
		using winrt::Microsoft::UI::Input::PointerDeviceType;
		const auto deviceType = e.Pointer().PointerDeviceType();
		if (deviceType == PointerDeviceType::Mouse || deviceType == PointerDeviceType::Pen)
		{
			uiElement.CapturePointer(e.Pointer());
		}
	}

	void RivePlayer::HandlePointerReleasedEvent(
		winrt::IInspectable const& sender,
		winrt::PointerRoutedEventArgs const& e)
	{
		const auto uiElement = sender.as<winrt::UIElement>();
		const auto pointerPos = e.GetCurrentPoint(uiElement).Position();
		_renderer->PointerUp(pointerPos.X, pointerPos.Y);

		if (_pointerDown)
		{
			// Cleared before the release: ReleasePointerCaptures raises
			// CaptureLost synchronously, and that must not send a second up.
			_pointerDown = false;
			uiElement.ReleasePointerCaptures();
		}
	}

	void RivePlayer::HandlePointerCaptureLostEvent(
		winrt::IInspectable const& sender,
		winrt::PointerRoutedEventArgs const& e)
	{
		// The capture went to another element, or the window lost focus, and no
		// Released is coming.
		EndInterruptedPress(sender, e);
	}

	void RivePlayer::HandlePointerCanceledEvent(
		winrt::IInspectable const& sender,
		winrt::PointerRoutedEventArgs const& e)
	{
		// A touch taken over by a system gesture: it never gets a Released.
		EndInterruptedPress(sender, e);
	}

	void RivePlayer::EndInterruptedPress(
		winrt::IInspectable const& sender,
		winrt::PointerRoutedEventArgs const& e)
	{
		if (!_pointerDown)
		{
			return;
		}
		_pointerDown = false;

		const auto uiElement = sender.as<winrt::UIElement>();
		const auto pointerPos = e.GetCurrentPoint(uiElement).Position();
		_renderer->PointerUp(pointerPos.X, pointerPos.Y);
	}
}
