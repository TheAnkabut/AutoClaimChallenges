// RLUtils.h - General utilities for RL modules
#pragma once
#include <string>
#include <Windows.h>

namespace RLUtils
{
    // String Conversion
    
    // Convert a wide string (UTF-16) to a UTF-8 std::string.
     
    inline std::string WideToUtf8(const std::wstring& wideStr)
    {
        if (wideStr.empty()) return "";
        int length = WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string utf8Str(length - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wideStr.c_str(), -1, utf8Str.data(), length, nullptr, nullptr);
        return utf8Str;
    }
    
    // Convert a UTF-8 std::string to a wide string (UTF-16).
    inline std::wstring Utf8ToWide(const std::string& utf8Str)
    {
        if (utf8Str.empty()) return L"";
        int length = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, nullptr, 0);
        std::wstring wideStr(length - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, wideStr.data(), length);
        return wideStr;
    }
}
