// src/windows/module.cpp
#include "windows/module.hpp"

namespace okkhor_windows {
namespace {

HMODULE g_module = nullptr;
volatile LONG g_ref_count = 0;

}  // namespace

void SetModuleHandle(HMODULE module) { g_module = module; }
HMODULE GetOwnModuleHandle() { return g_module; }

std::wstring GetModulePath() {
    if (!g_module) return {};
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        DWORD written = ::GetModuleFileNameW(g_module, path.data(),
                                             static_cast<DWORD>(path.size()));
        if (written == 0) return {};
        if (written < path.size()) {
            path.resize(written);
            return path;
        }
        // Truncated: grow and retry (long paths).
        path.resize(path.size() * 2);
    }
}

std::wstring GetModuleDirectory() {
    std::wstring path = GetModulePath();
    std::size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return {};
    return path.substr(0, slash);
}

void ModuleAddRef() { ::InterlockedIncrement(&g_ref_count); }
void ModuleRelease() { ::InterlockedDecrement(&g_ref_count); }
long ModuleRefCount() { return ::InterlockedCompareExchange(&g_ref_count, 0, 0); }

}  // namespace okkhor_windows
