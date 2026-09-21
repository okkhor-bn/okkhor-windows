// src/windows/unicode.hpp
//
// The one and only place where UTF-8 and UTF-16 meet.
//
// okkhor-core speaks UTF-8 (std::string); TSF and the rest of Win32 speak
// UTF-16 (std::wstring). Conversion happens here, through the Win32 conversion
// APIs -- never by casting bytes, and never by assuming one byte per character.
#pragma once

#include <string>

namespace okkhor_windows {

std::wstring Utf8ToWide(const std::string& utf8);
std::string WideToUtf8(const std::wstring& wide);

}  // namespace okkhor_windows
