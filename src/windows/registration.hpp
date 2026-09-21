// src/windows/registration.hpp
//
// Everything Windows needs to know about Okkhor before it will offer it as an
// input method. Three separate things must be registered, and all three are
// undone on uninstall:
//
//   1. the COM server         HKCR\CLSID\{clsid}\InprocServer32
//   2. the language profile   ITfInputProcessorProfiles::AddLanguageProfile
//   3. the TSF categories     ITfCategoryMgr::RegisterCategory
//
// (1) is per-machine and needs administrator rights. (2) and (3) are done
// through TSF's own APIs rather than by writing TSF's registry keys directly.
#pragma once

#include <windows.h>

namespace okkhor_windows {

// HKCR\CLSID\{clsid}, pointing at this DLL, ThreadingModel = Apartment.
HRESULT RegisterComServer();
HRESULT UnregisterComServer();

// Registers the text service with TSF and adds the Bangla language profile.
HRESULT RegisterProfile();
HRESULT UnregisterProfile();

// Declares what kind of text service this is: a keyboard TIP, and which
// capability categories it supports (secure desktops, immersive/Store apps,
// the system tray UI).
HRESULT RegisterCategories();
HRESULT UnregisterCategories();

}  // namespace okkhor_windows
