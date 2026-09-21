# okkhor-windows

A native Windows **Text Services Framework** (TSF) text service for
[okkhor-core](https://github.com/tajultonim/okkhor-core).

This repository is a platform adapter and nothing more. It contains no
tokenizer, no rules, no orthographic algebra, no renderer, and no special cases
for `w`, `kkh`, `t''` or anything else. Every one of those lives in the core.

```text
User keyboard input
        ↓
Windows TSF
        ↓
okkhor-windows          ← this repository: COM, TSF, UTF-16 ↔ UTF-8, composition
        ↓
okkhor-core             ← tokenizer → rules → algebra → renderer
        ↓
Bangla UTF-8
        ↓
TSF composition
        ↓
Application
```

**Status: phase 1.** The text service registers, appears in the Windows input
switcher, activates in TSF applications and loads okkhor-core. It does not yet
consume keys — that is phase 2. See [Roadmap](#roadmap).

---

## Build

Requires Visual Studio 2019 or newer (the Windows SDK supplies `msctf.h`) and
CMake 3.21+.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Debug
```

The core is fetched automatically. For side-by-side development against a local
checkout:

```powershell
cmake -S . -B build -A x64 -DOKKHOR_CORE_SOURCE_DIR=E:/programming/okkhor-core
```

The core's `data\` directory is copied next to `okkhor_tsf.dll` after every
build.

Options: `OKKHOR_WINDOWS_ENABLE_LOGGING` (ON), `OKKHOR_WINDOWS_STATIC_RUNTIME`
(ON — a text service is loaded into arbitrary processes, so depending on the VC
redistributable being present is a poor bet), `OKKHOR_WINDOWS_BUILD_TESTS` (ON).

```powershell
ctest --test-dir build -R engine_host --output-on-failure
```

`-R engine_host` is deliberate: adding okkhor-core as a subdirectory also
registers the core's own tests, whose executables this build does not produce.

## Install

From an **elevated** PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\install.ps1 -Dll build\Debug\okkhor_tsf.dll
powershell -ExecutionPolicy Bypass -File scripts\uninstall.ps1 -Dll build\Debug\okkhor_tsf.dll
```

The script calls `regsvr32`, which calls `DllRegisterServer`, which registers
the COM server, the Bangla language profile and the TSF categories in one step.
No manual registry editing. It also records the data directory under
`HKCU\Software\Okkhor\DataDir`.

Then: Settings → Time & language → Language & region → Bangla (Bangladesh) →
Language options → Keyboards → **Okkhor Phonetic**.

A 64-bit DLL serves 64-bit applications only. To use Okkhor in 32-bit
applications, configure with `-A Win32` and register that DLL too; both share
the same CLSID and profile GUID.

---

## Design

### Identity

| | |
| --- | --- |
| CLSID | `{6D27EED4-8B19-4F29-9046-720F559941E9}` |
| Profile GUID | `{DB2F1C82-2B00-4DCF-B81C-90CF10527D6D}` |
| Language | `0x0845` — Bengali (Bangladesh) |
| Categories | keyboard TIP, secure mode, UI element, immersive, systray, COM-less, input-mode compartment |

### How the TSF pieces fit together

```text
          Windows / application
                    │
      ITfTextInputProcessorEx::ActivateEx
                    ▼
          OkkhorTextService  ──────────────► ITfThreadMgr
                    │                            │
                    │  AdviseKeyEventSink        │  AdviseSink
                    ▼                            ▼
             ITfKeyEventSink            ITfThreadMgrEventSink
                    │                            │
           OnTestKeyDown / OnKeyDown      OnSetFocus, OnPopContext
                    │                            │
                    ▼                            ▼
              Composition  ◄──────────── cancel on focus change
            (latin_input, rendered)
                    │
                    │ engine.transliterate(latin_input)
                    ▼
            okkhor::Engine  ──►  Bangla UTF-8
                    │
                    │ RequestEditSession(TF_ES_READWRITE)
                    ▼
             ITfEditSession::DoEditSession
                    │
                    ▼
        ITfContext ─► ITfComposition ─► document
```

* **`ITfTextInputProcessorEx`** — created once per TSF thread. On `ActivateEx`
  it keeps the `ITfThreadMgr` and the `TfClientId`, loads the core, and (phase 2)
  advises the sinks. `Deactivate` undoes all of it.
* **`ITfKeyEventSink`** — `OnTestKeyDown` answers whether a key *would* be
  consumed without changing anything; `OnKeyDown` does the work. A key is
  consumed only when it belongs to the composition, so `Ctrl+C`, `Ctrl+V`,
  `Alt+Tab` and the Windows key pass straight through.
* **`ITfContext`** — the document the user is editing. Obtained from the thread
  manager's focus document manager, never cached across focus changes.
* **`ITfEditSession`** — nothing touches the document outside one. A key event
  arrives without a document lock, so `OnKeyDown` calls
  `ITfContext::RequestEditSession` with `TF_ES_SYNC | TF_ES_READWRITE`; TSF
  calls back into `DoEditSession` with a lock cookie, and only there do we start,
  update, commit or cancel a composition.
* **`ITfComposition`** — one live composition per active Latin buffer, started
  by `ITfContextComposition::StartComposition`. Every keystroke *replaces the
  composition's text*, it does not append a committed insertion, so the user
  sees আ → আম → আমি in a single editable run.
* **`ITfCompositionSink`** — `OnCompositionTerminated` fires when the
  application ends the composition behind our back (a click elsewhere, a focus
  change); the frontend drops its Latin buffer to stay in sync.

### The composition model

```text
latin_input  ── okkhor::Engine::transliterate ──►  rendered_text
   "ami"                                              "আমি"
```

The Latin buffer is the source of truth. Rendered Bangla is never parsed back
into Latin, and Backspace never cuts bytes off the Bangla string. Instead the
frontend asks the core to tokenize the Latin buffer and drops the last token's
worth of Latin (`EngineHost::LengthWithoutLastToken`), then re-renders the whole
buffer. Deleting from `kha` therefore leaves `kh` → খ, not a broken `kh` → `k`
plus a stray `h`.

### Layers

| path | responsibility |
| --- | --- |
| `src/core/engine_host.*` | Owns `okkhor::Engine`; turns core exceptions into return codes. **No Windows headers** — compiled and tested on any platform. |
| `src/windows/unicode.*` | The only UTF-8 ↔ UTF-16 conversion in the project, via `MultiByteToWideChar`. Malformed input is refused, never reinterpreted. |
| `src/windows/module.*` | Module handle, module directory, the `DllCanUnloadNow` reference count. |
| `src/windows/class_factory.*` | `IClassFactory` for the single CLSID. |
| `src/windows/registration.*` | COM server key, TSF language profile, TSF categories — and the exact inverse for uninstall. |
| `src/windows/dllmain.cpp` | `DllMain`, `DllGetClassObject`, `DllCanUnloadNow`, `DllRegisterServer`, `DllUnregisterServer`. |
| `src/tsf/text_service.*` | The TSF text service COM object. |
| `src/app/configuration.*` | GUIDs, language id, display names, data-directory resolution. |
| `src/util/log.*` | Development log. |

### Data directory

A text service is loaded into other people's processes, so the working
directory is meaningless. `ResolveDataDirectory()` looks at, in order:
`HKCU\Software\Okkhor\DataDir`, `%OKKHOR_DATA%`, then `data\`, `..\data\` and
`..\..\data\` relative to **the DLL's own location** (`GetModuleFileNameW`).

Note that the core's own `find_data_dir()` is not used from the Windows layer:
it would happily return the `OKKHOR_DATA_DIR` path baked in at compile time,
which is a build-machine path with no meaning on a user's computer.

### Threading and COM

TSF calls into the text service on the host application's UI thread, in a
single-threaded apartment (`ThreadingModel = Apartment`). One
`OkkhorTextService` exists per TSF thread and each owns its own engine, so
there is no shared mutable state and no cross-thread document access. Interfaces
are held in `Microsoft::WRL::ComPtr`; the objects' own reference counts are
manipulated only where the COM contract demands it (`delete this` in `Release`).

### Error handling

A text service that throws corrupts the host application. `EngineHost` catches
everything the core can raise and reports failure by return value; on a failed
transliteration the previous rendering is kept rather than pushing malformed
text into the document. Activation succeeds even if the data directory is
missing — the service simply does not transliterate, and says so in the log.

### Logging

`%LOCALAPPDATA%\Okkhor\okkhor-windows.log`, compiled in only when
`OKKHOR_WINDOWS_ENABLE_LOGGING` is on. Typed text is written through a separate
call (`OKKHOR_LOG_TEXT`) so it is easy to find and easy to strip: the contents
of someone's password field are not ours to record.

---

## Roadmap

| phase | scope | state |
| --- | --- | --- |
| 1 | COM server, registration, language profile, activation, core loading, logging | **done** |
| 2 | `ITfThreadMgrEventSink` + `ITfKeyEventSink`; `src/tsf/key_event_sink.*`, `src/tsf/thread_manager_event_sink.*` | next |
| 3 | Composition via edit sessions; `src/tsf/composition.*`, `src/tsf/edit_session.*` | |
| 4 | Drive `okkhor::Engine` from the live Latin buffer | |
| 5 | Commit / cancel / Backspace / Space / Enter / Escape / arrows / Delete | |
| 6 | Display attributes (underline the composition), icon resource, installer | |
| 7 | Testing across Notepad, Win32 edit controls, browsers, WinUI/Store apps | |
| 8 | Packaging, per-user (non-elevated) registration, telemetry-free diagnostics | |

Application-specific TSF quirks get documented here as they are found, rather
than patched around in the key handler.

## License

GPL-3.0, the same as okkhor-core, which this links against.
