// src/windows/module.hpp
//
// Process-wide state owned by the DLL itself: the module handle, the COM
// lifetime counter behind DllCanUnloadNow, and the module's directory.
#pragma once

#include <windows.h>

#include <string>

namespace okkhor_windows {

void SetModuleHandle(HMODULE module);
HMODULE GetOwnModuleHandle();

// Full path of this DLL, and the directory containing it.
std::wstring GetModulePath();
std::wstring GetModuleDirectory();

// COM server lifetime. Every live object and every IClassFactory::LockServer
// lock holds one count; DllCanUnloadNow returns S_OK only at zero.
void ModuleAddRef();
void ModuleRelease();
long ModuleRefCount();

// RAII helper for scopes that must keep the DLL loaded.
class ModuleLock {
public:
    ModuleLock() { ModuleAddRef(); }
    ~ModuleLock() { ModuleRelease(); }
    ModuleLock(const ModuleLock&) = delete;
    ModuleLock& operator=(const ModuleLock&) = delete;
};

}  // namespace okkhor_windows
