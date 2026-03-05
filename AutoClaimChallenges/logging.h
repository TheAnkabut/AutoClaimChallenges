// ReSharper disable CppNonExplicitConvertingConstructor
#pragma once
#include <string>
#include <source_location>
#include <format>
#include <memory>
#include <fstream>

#include "bakkesmod/wrappers/cvarmanagerwrapper.h"

extern std::shared_ptr<CVarManagerWrapper> _globalCvarManager;
#define DEBUG_LOG 0
#if DEBUG_LOG
constexpr const char* LOG_FILE_PATH = "C:\\path\\to\\log.txt";
#endif

struct FormatString
{
	std::string_view str;
	std::source_location loc{};

	FormatString(const char* str, const std::source_location& loc = std::source_location::current()) : str(str), loc(loc)
	{
	}

	FormatString(const std::string&& str, const std::source_location& loc = std::source_location::current()) : str(str), loc(loc)
	{
	}

	[[nodiscard]] std::string GetLocation() const
	{
		return std::format("[{} ({}:{})]", loc.function_name(), loc.file_name(), loc.line());
	}
};

struct FormatWstring
{
	std::wstring_view str;
	std::source_location loc{};

	FormatWstring(const wchar_t* str, const std::source_location& loc = std::source_location::current()) : str(str), loc(loc)
	{
	}

	FormatWstring(const std::wstring&& str, const std::source_location& loc = std::source_location::current()) : str(str), loc(loc)
	{
	}

	[[nodiscard]] std::wstring GetLocation() const
	{
		auto basic_string = std::format("[{} ({}:{})]", loc.function_name(), loc.file_name(), loc.line());
		return std::wstring(basic_string.begin(), basic_string.end());
	}
};

template <typename... Args>
void LOG(std::string_view format_str, Args&&... args)
{
#if DEBUG_LOG
	auto msg = std::vformat(format_str, std::make_format_args(args...));
	_globalCvarManager->log(msg);
	{
		std::ofstream f(LOG_FILE_PATH, std::ios::app);
		if (f.is_open())
			f << msg << std::endl;
	}
#endif
}

template <typename... Args>
void LOG(std::wstring_view format_str, Args&&... args)
{
#if DEBUG_LOG
	auto msg = std::vformat(format_str, std::make_wformat_args(args...));
	_globalCvarManager->log(msg);
	{
		std::wofstream f(LOG_FILE_PATH, std::ios::app);
		if (f.is_open())
			f << msg << std::endl;
	}
#endif
}

template <typename... Args>
void DEBUGLOG(const FormatString& format_str, Args&&... args)
{
#if DEBUG_LOG
	{
		auto text = std::vformat(format_str.str, std::make_format_args(args...));
		auto location = format_str.GetLocation();
		_globalCvarManager->log(std::format("{} {}", text, location));
	}
#endif
}

template <typename... Args>
void DEBUGLOG(const FormatWstring& format_str, Args&&... args)
{
#if DEBUG_LOG
	{
		auto text = std::vformat(format_str.str, std::make_wformat_args(args...));
		auto location = format_str.GetLocation();
		_globalCvarManager->log(std::format(L"{} {}", text, location));
	}
#endif
}
