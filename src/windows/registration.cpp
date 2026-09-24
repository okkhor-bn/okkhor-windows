// src/windows/registration.cpp
#include "windows/registration.hpp"

#include <msctf.h>
#include <wrl/client.h>

#include <cwchar>
#include <string>
#include <vector>

#include "app/configuration.hpp"
#include "util/log.hpp"
#include "windows/module.hpp"

using Microsoft::WRL::ComPtr;

namespace okkhor_windows
{
    namespace
    {

        std::wstring GuidToString(REFGUID guid)
        {
            wchar_t buffer[64] = {};
            ::StringFromGUID2(guid, buffer, ARRAYSIZE(buffer));
            return buffer;
        }

        std::wstring ClsidKeyPath() { return L"CLSID\\" + GuidToString(kOkkhorTextServiceClsid); }

        LONG SetStringValue(HKEY key, const wchar_t *name, const std::wstring &value)
        {
            return ::RegSetValueExW(key, name, 0, REG_SZ,
                                    reinterpret_cast<const BYTE *>(value.c_str()),
                                    static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
        }

        // RegDeleteTreeW needs the parent key; small wrapper so both unregister paths
        // behave the same when a key is already gone.
        LONG DeleteKeyTree(HKEY root, const std::wstring &path)
        {
            LONG result = ::RegDeleteTreeW(root, path.c_str());
            if (result == ERROR_FILE_NOT_FOUND)
                return ERROR_SUCCESS;
            return result;
        }

        // The capability categories a modern keyboard TIP should declare. Without
        // IMMERSIVESUPPORT the text service is unavailable in Store/WinUI apps; without
        // SYSTRAYSUPPORT it is missing from the language bar.
        const GUID *const kCategories[] = {
            &GUID_TFCAT_TIP_KEYBOARD,
            &GUID_TFCAT_TIPCAP_SECUREMODE,
            &GUID_TFCAT_TIPCAP_UIELEMENTENABLED,
            &GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,
            &GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,
            &GUID_TFCAT_TIPCAP_COMLESS,
            &GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT,
        };

    } // namespace

    HRESULT RegisterComServer()
    {
        const std::wstring module_path = GetModulePath();
        if (module_path.empty())
            return E_UNEXPECTED;

        HKEY clsid_key = nullptr;
        LONG result = ::RegCreateKeyExW(HKEY_CLASSES_ROOT, ClsidKeyPath().c_str(), 0, nullptr,
                                        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &clsid_key,
                                        nullptr);
        if (result != ERROR_SUCCESS)
            return HRESULT_FROM_WIN32(result);

        SetStringValue(clsid_key, nullptr, kClsidRegistryDescription);

        HKEY inproc_key = nullptr;
        result = ::RegCreateKeyExW(clsid_key, L"InprocServer32", 0, nullptr, REG_OPTION_NON_VOLATILE,
                                   KEY_WRITE, nullptr, &inproc_key, nullptr);
        if (result == ERROR_SUCCESS)
        {
            SetStringValue(inproc_key, nullptr, module_path);
            // A TSF text service lives on the host application's UI thread.
            SetStringValue(inproc_key, L"ThreadingModel", L"Apartment");
            ::RegCloseKey(inproc_key);
        }
        ::RegCloseKey(clsid_key);
        return HRESULT_FROM_WIN32(result);
    }

    HRESULT UnregisterComServer()
    {
        return HRESULT_FROM_WIN32(DeleteKeyTree(HKEY_CLASSES_ROOT, ClsidKeyPath()));
    }

    HRESULT RegisterProfile()
    {
        ComPtr<ITfInputProcessorProfiles> profiles;
        HRESULT hr = ::CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                        IID_PPV_ARGS(&profiles));
        if (FAILED(hr))
            return hr;

        hr = profiles->Register(kOkkhorTextServiceClsid);
        if (FAILED(hr))
            return hr;

        const std::wstring module_path = GetModulePath();

        // The icon index is 0 and the icon file is the DLL itself; with no icon
        // resource present Windows falls back to a generic input-method icon.
        hr = profiles->AddLanguageProfile(
            kOkkhorTextServiceClsid, kOkkhorLangId, kOkkhorProfileGuid, kProfileDescription,
            static_cast<ULONG>(wcslen(kProfileDescription)), module_path.c_str(),
            static_cast<ULONG>(module_path.size()), 0);
        return hr;
    }

    HRESULT UnregisterProfile()
    {
        ComPtr<ITfInputProcessorProfiles> profiles;
        HRESULT hr = ::CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                        IID_PPV_ARGS(&profiles));
        if (FAILED(hr))
            return hr;

        // Unregister removes the profiles belonging to the CLSID as well.
        return profiles->Unregister(kOkkhorTextServiceClsid);
    }

    HRESULT RegisterCategories()
    {
        ComPtr<ITfCategoryMgr> category_mgr;
        HRESULT hr = ::CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                        IID_PPV_ARGS(&category_mgr));
        if (FAILED(hr))
            return hr;

        for (const GUID *category : kCategories)
        {
            hr = category_mgr->RegisterCategory(kOkkhorTextServiceClsid, *category,
                                                kOkkhorTextServiceClsid);
            if (FAILED(hr))
                return hr;
        }
        return S_OK;
    }

    HRESULT UnregisterCategories()
    {
        ComPtr<ITfCategoryMgr> category_mgr;
        HRESULT hr = ::CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                        IID_PPV_ARGS(&category_mgr));
        if (FAILED(hr))
            return hr;

        for (const GUID *category : kCategories)
            category_mgr->UnregisterCategory(kOkkhorTextServiceClsid, *category,
                                             kOkkhorTextServiceClsid);
        return S_OK;
    }

} // namespace okkhor_windows
