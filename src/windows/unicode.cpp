// src/windows/unicode.cpp
#include "windows/unicode.hpp"

#include <windows.h>

namespace okkhor_windows {

std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return {};

    const int needed = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                                             static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) return {};  // Malformed UTF-8: refuse rather than emit garbage.

    std::wstring wide(static_cast<std::size_t>(needed), L'\0');
    const int written = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                                              static_cast<int>(utf8.size()), wide.data(), needed);
    if (written <= 0) return {};
    wide.resize(static_cast<std::size_t>(written));
    return wide;
}

std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};

    const int needed = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                             nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};

    std::string utf8(static_cast<std::size_t>(needed), '\0');
    const int written = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                              utf8.data(), needed, nullptr, nullptr);
    if (written <= 0) return {};
    utf8.resize(static_cast<std::size_t>(written));
    return utf8;
}

}  // namespace okkhor_windows
