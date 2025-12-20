#pragma once
#include <string>
#include <string_view>
#include <windows.h>
#include <stdexcept>

namespace winrt::XamlToolkit::WinUI::Rive::implementation
{
    struct Encoding 
    {
        static std::string utf16_to_utf8(std::wstring_view utf16)
        {
            if (utf16.empty())
                return {};

            const int requiredSize = ::WideCharToMultiByte(
                CP_UTF8,
                WC_ERR_INVALID_CHARS,
                utf16.data(),
                static_cast<int>(utf16.size()),
                nullptr,
                0,
                nullptr,
                nullptr
            );

            if (requiredSize <= 0)
                throw std::runtime_error("WideCharToMultiByte size query failed");

            std::string utf8(requiredSize, '\0');

            const int written = ::WideCharToMultiByte(
                CP_UTF8,
                WC_ERR_INVALID_CHARS,
                utf16.data(),
                static_cast<int>(utf16.size()),
                utf8.data(),
                requiredSize,
                nullptr,
                nullptr
            );

            if (written != requiredSize)
                throw std::runtime_error("WideCharToMultiByte conversion failed");

            return utf8;
        }
    };
}