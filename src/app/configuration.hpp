// src/app/configuration.hpp
//
// Every identity constant of the text service lives here: the CLSID, the
// language profile GUID, the language the profile is registered under, and the
// display strings. Nothing else in the project should hardcode a GUID.
#pragma once

#include <windows.h>

#include <string>

namespace okkhor_windows {

// {6D27EED4-8B19-4F29-9046-720F559941E9}
extern const CLSID kOkkhorTextServiceClsid;

// {DB2F1C82-2B00-4DCF-B81C-90CF10527D6D}
extern const GUID kOkkhorProfileGuid;

// Bengali (Bangladesh). The profile is registered under this language so that
// Windows offers it in the input-language switcher independently of whatever
// keyboard layout the user currently has.
constexpr LANGID kOkkhorLangId = MAKELANGID(LANG_BENGALI, SUBLANG_BENGALI_BANGLADESH);

constexpr wchar_t kTextServiceDescription[] = L"Okkhor (Bangla Phonetic)";
constexpr wchar_t kProfileDescription[] = L"Okkhor Phonetic";
constexpr wchar_t kClsidRegistryDescription[] = L"Okkhor Bangla Phonetic Text Service";

// Resolves the okkhor-core data directory (vowels.json, rules.json, ...).
//
// The working directory of a process that loads a text service DLL is the host
// application's, which is meaningless to us, so resolution is anchored to the
// DLL's own location and to an explicit registry/environment override instead.
//
// Order:
//   1. HKCU\Software\Okkhor\DataDir           (per-user override)
//   2. %OKKHOR_DATA%                          (developer override)
//   3. <dll directory>\data                   (normal install layout)
//   4. <dll directory>\..\data                (build tree layout)
//   5. <dll directory>\..\..\data
//
// Returns an empty string when no candidate contains vowels.json.
std::wstring ResolveDataDirectory();

}  // namespace okkhor_windows
