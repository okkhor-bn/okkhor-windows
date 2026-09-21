// src/app/configuration.cpp
//
// Definitions of the identity constants and the data-directory search.
#include "app/configuration.hpp"

#include <shlwapi.h>

#include <vector>

#include "windows/module.hpp"

namespace okkhor_windows {

// {6D27EED4-8B19-4F29-9046-720F559941E9}
const CLSID kOkkhorTextServiceClsid = {
    0x6d27eed4, 0x8b19, 0x4f29, {0x90, 0x46, 0x72, 0x0f, 0x55, 0x99, 0x41, 0xe9}};

// {DB2F1C82-2B00-4DCF-B81C-90CF10527D6D}
const GUID kOkkhorProfileGuid = {
    0xdb2f1c82, 0x2b00, 0x4dcf, {0xb8, 0x1c, 0x90, 0xcf, 0x10, 0x52, 0x7d, 0x6d}};

namespace {

bool LooksLikeDataDir(const std::wstring& dir) {
    if (dir.empty()) return false;
    std::wstring probe = dir;
    if (probe.back() != L'\\' && probe.back() != L'/') probe += L'\\';
    probe += L"vowels.json";
    DWORD attrs = ::GetFileAttributesW(probe.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring ReadRegistryOverride() {
    HKEY key = nullptr;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Okkhor", 0, KEY_READ, &key) != ERROR_SUCCESS)
        return {};
    std::wstring value;
    DWORD type = 0;
    DWORD size = 0;
    if (::RegQueryValueExW(key, L"DataDir", nullptr, &type, nullptr, &size) == ERROR_SUCCESS &&
        (type == REG_SZ || type == REG_EXPAND_SZ) && size > sizeof(wchar_t)) {
        std::vector<wchar_t> buffer(size / sizeof(wchar_t) + 1, L'\0');
        if (::RegQueryValueExW(key, L"DataDir", nullptr, nullptr,
                               reinterpret_cast<LPBYTE>(buffer.data()), &size) == ERROR_SUCCESS) {
            value.assign(buffer.data());
        }
    }
    ::RegCloseKey(key);
    return value;
}

std::wstring ReadEnvironmentOverride() {
    DWORD needed = ::GetEnvironmentVariableW(L"OKKHOR_DATA", nullptr, 0);
    if (needed == 0) return {};
    std::wstring value(needed, L'\0');
    DWORD written = ::GetEnvironmentVariableW(L"OKKHOR_DATA", value.data(), needed);
    value.resize(written);
    return value;
}

std::wstring Combine(const std::wstring& base, const wchar_t* relative) {
    std::wstring path = base;
    if (!path.empty() && path.back() != L'\\') path += L'\\';
    path += relative;

    wchar_t canonical[MAX_PATH] = {};
    if (::PathCanonicalizeW(canonical, path.c_str())) return canonical;
    return path;
}

}  // namespace

std::wstring ResolveDataDirectory() {
    std::vector<std::wstring> candidates;

    if (std::wstring reg = ReadRegistryOverride(); !reg.empty()) candidates.push_back(reg);
    if (std::wstring env = ReadEnvironmentOverride(); !env.empty()) candidates.push_back(env);

    const std::wstring module_dir = GetModuleDirectory();
    if (!module_dir.empty()) {
        candidates.push_back(Combine(module_dir, L"data"));
        candidates.push_back(Combine(module_dir, L"..\\data"));
        candidates.push_back(Combine(module_dir, L"..\\..\\data"));
    }

    for (const std::wstring& candidate : candidates)
        if (LooksLikeDataDir(candidate)) return candidate;

    return {};
}

}  // namespace okkhor_windows
