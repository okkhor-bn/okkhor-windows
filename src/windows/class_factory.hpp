// src/windows/class_factory.hpp
//
// The IClassFactory that COM asks for when Windows wants to create an instance
// of the Okkhor text service for a thread.
#pragma once

#include <unknwn.h>
#include <windows.h>

namespace okkhor_windows {

class OkkhorClassFactory : public IClassFactory {
public:
    OkkhorClassFactory();

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override;
    STDMETHODIMP LockServer(BOOL lock) override;

private:
    ~OkkhorClassFactory();

    LONG ref_count_;
};

}  // namespace okkhor_windows
