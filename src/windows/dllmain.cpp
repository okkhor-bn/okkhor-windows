// src/windows/dllmain.cpp
//
// The DLL entry point and the four exports every in-process COM server must
// provide. Nothing here knows anything about TSF beyond handing out the class
// factory and calling the registration helpers.
#include <objbase.h>
#include <windows.h>

#include <new>

#include "app/configuration.hpp"
#include "util/log.hpp"
#include "windows/class_factory.hpp"
#include "windows/module.hpp"
#include "windows/registration.hpp"

using namespace okkhor_windows;

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID /*reserved*/) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            SetModuleHandle(module);
            // We do no per-thread work, and DllMain must stay minimal.
            ::DisableThreadLibraryCalls(module);
            break;
        case DLL_PROCESS_DETACH:
            log::Shutdown();
            break;
        default:
            break;
    }
    return TRUE;
}

// Hands COM the factory for our one and only class.
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (!ppv) return E_INVALIDARG;
    *ppv = nullptr;

    if (!IsEqualCLSID(rclsid, kOkkhorTextServiceClsid)) return CLASS_E_CLASSNOTAVAILABLE;

    auto* factory = new (std::nothrow) OkkhorClassFactory();
    if (!factory) return E_OUTOFMEMORY;

    const HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

// COM may unload us once nothing is alive and nothing holds a server lock.
STDAPI DllCanUnloadNow() { return ModuleRefCount() == 0 ? S_OK : S_FALSE; }

// regsvr32 okkhor_tsf.dll   (elevated)
STDAPI DllRegisterServer() {
    HRESULT hr = RegisterComServer();
    if (SUCCEEDED(hr)) hr = RegisterProfile();
    if (SUCCEEDED(hr)) hr = RegisterCategories();

    if (FAILED(hr)) {
        // Leave nothing half-registered behind.
        UnregisterCategories();
        UnregisterProfile();
        UnregisterComServer();
    }
    return hr;
}

// regsvr32 /u okkhor_tsf.dll   (elevated)
STDAPI DllUnregisterServer() {
    // Unregister in reverse order and keep going even if a step is already
    // undone, so a partially installed service can still be cleaned up.
    UnregisterCategories();
    UnregisterProfile();
    UnregisterComServer();
    return S_OK;
}
