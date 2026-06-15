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
#include <filesystem>
#include <fstream>
#endif

#include "Encoding.h"
#include "StateMachineInputCollection.h"

namespace winrt
{
	using namespace Windows::Foundation;
	using namespace Windows::Storage;
	using namespace Windows::Web::Http;
	using namespace Windows::UI::Xaml::Interop;
	using namespace Microsoft::UI::Xaml;
	using namespace Microsoft::UI::Xaml::Controls;
}

namespace winrt::XamlToolkit::WinUI::Rive::implementation
{
	const wil::single_threaded_property<winrt::DependencyProperty> RivePlayer::SourceProperty = winrt::DependencyProperty::Register(
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

	const wil::single_threaded_property<winrt::DependencyProperty> RivePlayer::ArtboardProperty = winrt::DependencyProperty::Register(
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

	const wil::single_threaded_property<winrt::DependencyProperty> RivePlayer::StateMachineProperty = winrt::DependencyProperty::Register(
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

	const wil::single_threaded_property<winrt::DependencyProperty> RivePlayer::StateMachineInputCollectionProperty = winrt::DependencyProperty::Register(
		L"StateMachineInputCollection",
		winrt::xaml_typename<winrt::XamlToolkit::WinUI::Rive::StateMachineInputCollection>(),
		winrt::xaml_typename<class_type>(),
		winrt::PropertyMetadata(winrt::make<implementation::StateMachineInputCollection>(), &RivePlayer::OnStateMachineInputCollectionChanged));

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
		PointerPressed({ this, &RivePlayer::HandlePointerPressedEvent });
		PointerReleased({ this, &RivePlayer::HandlePointerReleasedEvent });
		_renderer = std::make_unique<RiveRenderer>();
		auto collection = StateMachineInputCollection();
		auto collectionImpl = winrt::get_self<implementation::StateMachineInputCollection>(collection);
		collectionImpl->SetRivePlayer(*this);
	}

	void RivePlayer::OnApplyTemplate()
	{
		_swapChainPanel = GetTemplateChild(ContainerVisualName).try_as<winrt::SwapChainPanel>();
	}

	void RivePlayer::SetBool(winrt::hstring const& name, bool value)
	{
		auto utf8Name = Encoding::utf16_to_utf8(name);
		if (_deferredSMInputsDuringAsyncSourceLoad)
		{
			// A source file is currently loading async. Don't set this input until it completes.
			_deferredSMInputsDuringAsyncSourceLoad->emplace_back(
				InputCommand{ .name = utf8Name, .kind = InputCommand::Kind::Bool, .boolValue = value });
		}
		else
		{
			_renderer->SetBoolInput(utf8Name, value);
		}
	}

	void RivePlayer::SetNumber(winrt::hstring const& name, float value)
	{
		auto utf8Name = Encoding::utf16_to_utf8(name);
		if (_deferredSMInputsDuringAsyncSourceLoad)
		{
			// A source file is currently loading async. Don't set this input until it completes.
			_deferredSMInputsDuringAsyncSourceLoad->emplace_back(
				InputCommand{ .name = utf8Name, .kind = InputCommand::Kind::Number, .numberValue = value });
		}
		else
		{
			_renderer->SetNumberInput(utf8Name, value);
		}
	}

	void RivePlayer::FireTrigger(winrt::hstring const& name)
	{
		auto utf8Name = Encoding::utf16_to_utf8(name);
		if (_deferredSMInputsDuringAsyncSourceLoad)
		{
			// A source file is currently loading async. Don't set this input until it completes.
			_deferredSMInputsDuringAsyncSourceLoad->emplace_back(
				InputCommand{ .name = utf8Name, .kind = InputCommand::Kind::Trigger });
		}
		else
		{
			_renderer->FireTrigger(utf8Name);
		}
	}

	void RivePlayer::OnLoaded(
		[[maybe_unused]] winrt::IInspectable const& s,
		[[maybe_unused]] winrt::RoutedEventArgs const& e)
	{
		if (_swapChainPanel)
		{
			auto size = _swapChainPanel.ActualSize();
			winrt::com_ptr<ISwapChainPanelNative> panelNative = _swapChainPanel.as<ISwapChainPanelNative>();
			_renderer->Initialize(panelNative, static_cast<int>(size.x), static_cast<int>(size.y));
			_renderer->Start();
		}
	}

	void RivePlayer::OnUnloaded(
		[[maybe_unused]] winrt::IInspectable const& s,
		[[maybe_unused]] winrt::RoutedEventArgs const& e)
	{
		_renderer->Stop();
	}

	void RivePlayer::OnSourceNameChanged(
		winrt::DependencyObject const& d,
		winrt::DependencyPropertyChangedEventArgs const& e)
	{
		auto player = d.try_as<class_type>();
		auto playerImpl = winrt::get_self<RivePlayer>(player);
		auto newSourceName = winrt::unbox_value<winrt::hstring>(e.NewValue());
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
		auto player = d.try_as<class_type>();
		auto playerImpl = winrt::get_self<RivePlayer>(player);
		auto newArtboardName = winrt::unbox_value<winrt::hstring>(e.NewValue());
		auto utf8ArtboardName = Encoding::utf16_to_utf8(newArtboardName);
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
			playerImpl->_renderer->SelectArtboard(utf8ArtboardName);
		}
	}

	void RivePlayer::OnStateMachineNameChanged(
		winrt::DependencyObject const& d,
		winrt::DependencyPropertyChangedEventArgs const& e)
	{
		auto player = d.try_as<class_type>();
		auto playerImpl = winrt::get_self<RivePlayer>(player);
		auto newStateMachineName = winrt::unbox_value<winrt::hstring>(e.NewValue());
		auto utf8StateMachineName = Encoding::utf16_to_utf8(newStateMachineName);
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
			playerImpl->_renderer->SelectStateMachine(utf8StateMachineName);
		}
	}

	void RivePlayer::OnStateMachineInputCollectionChanged(
		winrt::DependencyObject const& d,
		winrt::DependencyPropertyChangedEventArgs const& e)
	{
		// Clear the RivePlayer on the old reference so it quits updating us.
		auto oldCollection = e.OldValue().try_as<winrt::XamlToolkit::WinUI::Rive::StateMachineInputCollection>();
		auto oldCollectionImpl = winrt::get_self<implementation::StateMachineInputCollection>(oldCollection);
		oldCollectionImpl->SetRivePlayer(nullptr);

		auto newCollection = e.NewValue().try_as<winrt::XamlToolkit::WinUI::Rive::StateMachineInputCollection>();
		auto newCollectionImpl = winrt::get_self<implementation::StateMachineInputCollection>(newCollection);
		newCollectionImpl->SetRivePlayer(d.try_as<class_type>());
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
		auto scheme = uri.SchemeName();
		if (scheme == L"http" || scheme == L"https")
		{
			try
			{
				winrt::HttpClient httpClient;
				winrt::HttpResponseMessage response = co_await httpClient.GetAsync(uri);
				if (response.IsSuccessStatusCode())
				{
					auto buffer = co_await response.Content().ReadAsBufferAsync();
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
			auto file = co_await winrt::StorageFile::GetFileFromApplicationUriAsync(uri);
			if (file != nullptr && sourceToken == _currentSourceToken)
			{
				auto buffer = co_await winrt::FileIO::ReadBufferAsync(file);
				data.resize(buffer.Length());
				std::copy(buffer.data(), buffer.data() + buffer.Length(), data.data());
			}
		}
		else if (scheme == L"file")
		{
			auto path = Encoding::utf16_to_utf8(uriString);
			if (auto fs = std::ifstream(path, std::ios::binary))
			{
				data.assign(std::istreambuf_iterator<char>(fs), {});
			}
		}

		if (!data.empty() && sourceToken == _currentSourceToken)
		{
			_renderer->LoadFileData(data);
			//// Apply deferred state machine inputs once the scene is fully loaded.
			for (auto& stateMachineInput : *_deferredSMInputsDuringAsyncSourceLoad)
			{
				_renderer->Enqueue(std::move(stateMachineInput));
			}
		}

		_deferredSMInputsDuringAsyncSourceLoad.reset();
	}

	void RivePlayer::HandleSizeChangedEvent(
		[[maybe_unused]] winrt::IInspectable const& sender,
		winrt::SizeChangedEventArgs const& e)
	{
		if (_renderer)
		{
			auto viewSize = e.NewSize();
			_renderer->Resize(static_cast<int>(viewSize.Width), static_cast<int>(viewSize.Height));
		}
	}

	void RivePlayer::HandlePointerMovedEvent(
		winrt::IInspectable const& sender,
		winrt::Input::PointerRoutedEventArgs const& e)
	{
		auto uiElement = sender.as<winrt::UIElement>();
		auto pointerPos = e.GetCurrentPoint(uiElement).Position();
		_renderer->PointerMove(pointerPos.X, pointerPos.Y);
	}

	void RivePlayer::HandlePointerPressedEvent(
		winrt::IInspectable const& sender,
		winrt::Input::PointerRoutedEventArgs const& e)
	{
		auto uiElement = sender.as<winrt::UIElement>();
		auto pointerPos = e.GetCurrentPoint(uiElement).Position();
		_renderer->PointerDown(pointerPos.X, pointerPos.Y);
	}

	void RivePlayer::HandlePointerReleasedEvent(
		winrt::IInspectable const& sender,
		winrt::Input::PointerRoutedEventArgs const& e)
	{
		auto uiElement = sender.as<winrt::UIElement>();
		auto pointerPos = e.GetCurrentPoint(uiElement).Position();
		_renderer->PointerUp(pointerPos.X, pointerPos.Y);
	}
}
