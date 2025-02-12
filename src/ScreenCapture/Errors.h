#pragma once
#include <comdef.h>
#include <sstream>
#include <iomanip>
#include <stdexcept>

inline std::wstring to_utf16(const std::string msg) {
    int len = MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), (int)msg.size(), NULL, NULL);
    if (len > 0) {
        WCHAR* buffer = new WCHAR[len + 1];
        MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), (int)msg.size(), buffer, len);
        buffer[len] = 0;
        return std::wstring(buffer);
    }
    return std::wstring();
}

inline void debug_hresult(const WCHAR* caption, HRESULT hr, bool throwException = true)
{
    if (hr != S_OK)
    {
        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        winrt::hstring message = winrt::to_hstring(caption);
        std::wostringstream wostringstream;
        wostringstream << winrt::to_hstring(caption) << L"failed with HRESULT: ";
        wostringstream << std::setfill(L'0') << std::setw(8) << std::hex << hr;
        wostringstream << L": " << errMsg << L"\r\n";
        std::wstring wideMessage = wostringstream.str();
        LPCTSTR wideChars = wideMessage.c_str();
        OutputDebugString(wideChars);
        wprintf(L"%s", wideChars);
        if (throwException) {
            throw winrt::hresult_error(hr, wideMessage);
        }
    }
}
