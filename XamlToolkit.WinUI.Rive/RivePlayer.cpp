#include "pch.h"
#include "RivePlayer.h"
#if __has_include("RivePlayer.g.cpp")
#include "RivePlayer.g.cpp"
#endif
#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include "Encoding.h"
#include <filesystem>
#include <fstream>
#include "StateMachineInputCollection.h"

namespace winrt::XamlToolkit::WinUI::Rive::implementation
{
	const wil::single_threaded_property<winrt::Microsoft::UI::Xaml::DependencyProperty> RivePlayer::SourceProperty = winrt::Microsoft::UI::Xaml::DependencyProperty::Register(
		L"Source",
		winrt::xaml_typename<winrt::hstring>(),
		winrt::xaml_typename<class_type>(),
		winrt::Microsoft::UI::Xaml::PropertyMetadata(winrt::box_value(L""), &RivePlayer::OnSourceNameChanged));

	winrt::hstring RivePlayer::Source() const
	{
		return winrt::unbox_value<winrt::hstring>(GetValue(SourceProperty()));
	}

	void RivePlayer::Source(winrt::hstring const& value) const
	{
		SetValue(SourceProperty(), winrt::box_value(value));
	}

	const wil::single_threaded_property<winrt::Microsoft::UI::Xaml::DependencyProperty> RivePlayer::ArtboardProperty = winrt::Microsoft::UI::Xaml::DependencyProperty::Register(
		L"Artboard",
		winrt::xaml_typename<winrt::hstring>(),
		winrt::xaml_typename<class_type>(),
		winrt::Microsoft::UI::Xaml::PropertyMetadata(winrt::box_value(L""), &RivePlayer::OnArtboardNameChanged));

	winrt::hstring RivePlayer::Artboard() const
	{
		return winrt::unbox_value<winrt::hstring>(GetValue(ArtboardProperty()));
	}

	void RivePlayer::Artboard(winrt::hstring const& value) const
	{
		SetValue(ArtboardProperty(), winrt::box_value(value));
	}

	const wil::single_threaded_property<winrt::Microsoft::UI::Xaml::DependencyProperty> RivePlayer::StateMachineProperty = winrt::Microsoft::UI::Xaml::DependencyProperty::Register(
		L"StateMachine",
		winrt::xaml_typename<winrt::hstring>(),
		winrt::xaml_typename<class_type>(),
		winrt::Microsoft::UI::Xaml::PropertyMetadata(winrt::box_value(L""), &RivePlayer::OnStateMachineNameChanged));

	winrt::hstring RivePlayer::StateMachine() const
	{
		return winrt::unbox_value<winrt::hstring>(GetValue(StateMachineProperty()));
	}

	void RivePlayer::StateMachine(winrt::hstring const& value) const
	{
		SetValue(StateMachineProperty(), winrt::box_value(value));
	}

	const wil::single_threaded_property<winrt::Microsoft::UI::Xaml::DependencyProperty> RivePlayer::StateMachineInputCollectionProperty = winrt::Microsoft::UI::Xaml::DependencyProperty::Register(
		L"StateMachineInputCollection",
		winrt::xaml_typename<winrt::XamlToolkit::WinUI::Rive::StateMachineInputCollection>(),
		winrt::xaml_typename<class_type>(),
		winrt::Microsoft::UI::Xaml::PropertyMetadata(winrt::make<implementation::StateMachineInputCollection>(), &RivePlayer::OnStateMachineInputCollectionChanged));

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
		_swapChainPanel = GetTemplateChild(ContainerVisualName).try_as<winrt::Microsoft::UI::Xaml::Controls::SwapChainPanel>();
	}

	void RivePlayer::SetBool(winrt::hstring const& name, bool value)
	{
		auto utf8Name = Encoding::utf16_to_utf8(name);
		if (_deferredSMInputsDuringAsyncSourceLoad)
		{
			// A source file is currently loading async. Don't set this input until it completes.
			_deferredSMInputsDuringAsyncSourceLoad->emplace_back(
				InputCmd{ .name = utf8Name, .kind = InputCmd::Kind::Bool, .b = value });
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
				InputCmd{ .name = utf8Name, .kind = InputCmd::Kind::Number, .f = value });
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
				InputCmd{ .name = utf8Name, .kind = InputCmd::Kind::Trigger });
		}
		else
		{
			_renderer->FireTrigger(utf8Name);
		}
	}

	void RivePlayer::OnLoaded(
		[[maybe_unused]] winrt::Windows::Foundation::IInspectable const& s,
		[[maybe_unused]] winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
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
		[[maybe_unused]] winrt::Windows::Foundation::IInspectable const& s,
		[[maybe_unused]] winrt::Microsoft::UI::Xaml::RoutedEventArgs const& e)
	{
		_renderer->Stop();
	}

	void RivePlayer::OnSourceNameChanged(
		winrt::Microsoft::UI::Xaml::DependencyObject const& d,
		winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& e)
	{
		auto player = d.try_as<class_type>();
		auto playerImpl = winrt::get_self<RivePlayer>(player);
		auto newSourceName = winrt::unbox_value<winrt::hstring>(e.NewValue());
		// Clear the current Scene while we wait for the new one to load.
		playerImpl->_renderer->Clear();
		++playerImpl->_currentSourceToken;  // Cancel any other active async source load operation.
		// Defer state machine inputs here until the new file is loaded.
		playerImpl->_deferredSMInputsDuringAsyncSourceLoad = std::make_unique<std::vector<Command>>();
		playerImpl->LoadSourceFileDataAsync(newSourceName, playerImpl->_currentSourceToken);
	}

	void RivePlayer::OnArtboardNameChanged(winrt::Microsoft::UI::Xaml::DependencyObject const& d,
		winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& e)
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
		winrt::Microsoft::UI::Xaml::DependencyObject const& d,
		winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& e)
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
		winrt::Microsoft::UI::Xaml::DependencyObject const& d,
		winrt::Microsoft::UI::Xaml::DependencyPropertyChangedEventArgs const& e)
	{
		// Clear the RivePlayer on the old reference so it quits updating us.
		auto oldCollection = e.OldValue().try_as<winrt::XamlToolkit::WinUI::Rive::StateMachineInputCollection>();
		auto oldCollectionImpl = winrt::get_self<implementation::StateMachineInputCollection>(oldCollection);
		oldCollectionImpl->SetRivePlayer(nullptr);

		auto newCollection = e.NewValue().try_as<winrt::XamlToolkit::WinUI::Rive::StateMachineInputCollection>();
		auto newCollectionImpl = winrt::get_self<implementation::StateMachineInputCollection>(newCollection);
		newCollectionImpl->SetRivePlayer(d.try_as<class_type>());
	}

	static winrt::Windows::Foundation::Uri TryCreate(winrt::hstring const& uriString)
	{
		try
		{
			winrt::Windows::Foundation::Uri uri(uriString);
			return uri;
		}
		catch ([[maybe_unused]] winrt::hresult_error const& ex)
		{
			return nullptr;
		}
	}

	winrt::Windows::Foundation::IAsyncAction RivePlayer::LoadSourceFileDataAsync(winrt::hstring const& uriString, int sourceToken)
	{
		winrt::Windows::Foundation::Uri uri = TryCreate(uriString);
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
				Windows::Web::Http::HttpClient httpClient;
				Windows::Web::Http::HttpResponseMessage response = co_await httpClient.GetAsync(uri);
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
			auto file = co_await Windows::Storage::StorageFile::GetFileFromApplicationUriAsync(uri);
			if (file != nullptr && sourceToken == _currentSourceToken)
			{
				auto buffer = co_await Windows::Storage::FileIO::ReadBufferAsync(file);
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
		[[maybe_unused]] winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::SizeChangedEventArgs const& e)
	{
		if (_renderer)
		{
			auto viewSize = e.NewSize();
			_renderer->Resize(static_cast<int>(viewSize.Width), static_cast<int>(viewSize.Height));
		}
	}

	void RivePlayer::HandlePointerMovedEvent(
		winrt::Windows::Foundation::IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e)
	{
		auto uiElement = sender.as<winrt::Microsoft::UI::Xaml::UIElement>();
		auto pointerPos = e.GetCurrentPoint(uiElement).Position();
		_renderer->PointerMove(pointerPos.X, pointerPos.Y);
	}

	void RivePlayer::HandlePointerPressedEvent(
		IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e)
	{
		auto uiElement = sender.as<winrt::Microsoft::UI::Xaml::UIElement>();
		auto pointerPos = e.GetCurrentPoint(uiElement).Position();
		_renderer->PointerDown(pointerPos.X, pointerPos.Y);
	}

	void RivePlayer::HandlePointerReleasedEvent(
		IInspectable const& sender,
		winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& e)
	{
		auto uiElement = sender.as<winrt::Microsoft::UI::Xaml::UIElement>();
		auto pointerPos = e.GetCurrentPoint(uiElement).Position();
		_renderer->PointerUp(pointerPos.X, pointerPos.Y);
	}
}
