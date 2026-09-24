# Okkhor for Windows

**Okkhor Phonetic** is a Bangla phonetic input method for Windows.

Type Bangla phonetically using Latin letters and Okkhor converts your input into Bangla script in real time.

For example:

```text
ami banglay likhi
```

becomes:

```text
আমি বাংলায় লিখি
```

Okkhor integrates with Windows through the **Text Services Framework (TSF)**, so it works as a native Windows keyboard/input method.

---

## Features

- Bangla phonetic typing
- Real-time Latin-to-Bangla transliteration
- Native Windows TSF integration
- Works across Windows applications that support TSF
- Backspace-aware composition editing
- Powered by [okkhor-core](https://github.com/okkhor-bn/okkhor-core)
- No Python, Node.js, Java, or other runtime required
- Prebuilt releases for end users

---

# Installation

## Requirements

- Windows 10 or later
- 64-bit Windows
- Administrator privileges
- Internet connection for the online installer

---

## Recommended Installation

The easiest way to install Okkhor is through the PowerShell installer.

Open **PowerShell** and run:

```powershell
irm https://raw.githubusercontent.com/okkhor-bn/okkhor-windows/main/scripts/install.ps1 | iex
```

The installer will automatically:

1. Detect your Windows architecture.
2. Download the latest stable Okkhor release.
3. Install the Okkhor TSF component.
4. Register Okkhor with Windows.
5. Restart the required Windows components.
6. Finish the installation.

You do **not** need to download or provide a DLL manually.

---

## Enable Okkhor Phonetic

After installation, open:

```text
Settings
→ Time & language
→ Language & region
→ Bangla (Bangladesh)
→ Language options
→ Keyboards
→ Add a keyboard
→ Okkhor Phonetic
```

After adding the keyboard, select **Okkhor Phonetic** from the Windows keyboard/language selector.

You can then start typing Bangla phonetically.

---

# Using Okkhor

Once Okkhor Phonetic is selected, type using Latin letters.

For example:

```text
ami
```

produces:

```text
আমি
```

You can continue typing naturally:

```text
ami banglay likhte pari
```

and Okkhor updates the Bangla composition as you type.

### Backspace

Backspace removes the most recently typed Latin input character and updates the Bangla composition accordingly.

### Spaces

A space commits the current composition and starts the next word.

---

# How It Works

Okkhor uses a rule-based transliteration engine.

The general processing pipeline is:

```text
                    Okkhor Windows
                          │
                          ▼
                    Windows TSF
                          │
                          ▼
                    okkhor-core
                          │
                          ▼
                  Bangla composition
```

The Windows component receives keyboard input and maintains the current Latin composition.

The transliteration work itself is performed by [okkhor-core](https://github.com/okkhor-bn/okkhor-core).

---

# Releases

Prebuilt Windows binaries are distributed through GitHub Releases.

A release contains the compiled Okkhor TSF component, for example:

```text
okkhor-windows-x64.zip
└── okkhor_tsf.dll
```

End users normally do not need to interact with these files directly.

The PowerShell installer downloads the appropriate release automatically.

---

# Manual Installation

Manual installation is primarily useful for developers or users who already have a compiled Okkhor DLL.

If you have:

```text
okkhor_tsf.dll
```

you can install it with:

```powershell
.\scripts\install.ps1 -Dll path\to\okkhor_tsf.dll
```

For example:

```powershell
.\scripts\install.ps1 -Dll build\Debug\okkhor_tsf.dll
```

The script must be run with administrator privileges.

---

# Uninstallation

Run:

```powershell
.\scripts\uninstall.ps1
```

The uninstaller removes the Okkhor TSF registration and installed components.

After uninstalling, Okkhor Phonetic will no longer appear as an available Windows keyboard.

---

# Development

## Repository Structure

```text
okkhor-windows/
│
├── CMakeLists.txt
│
├── external/
│   └── okkhor-core/
│
├── src/
│   ├── core/
│   │   ├── engine_host.cpp
│   │   └── engine_host.hpp
│   │
│   ├── tsf/
│   │   ├── text_service.cpp
│   │   ├── text_service.hpp
│   │   └── ...
│   │
│   ├── util/
│   │   └── ...
│   │
│   └── windows/
│       └── ...
│
├── tests/
│   └── engine_host_tests.cpp
│
└── scripts/
    ├── dev-build.ps1
    ├── install.ps1
    └── uninstall.ps1
```

`okkhor-core` is included as a Git submodule.

Initialize it after cloning:

```powershell
git submodule update --init --recursive
```

---

## Build

Configure the project:

```powershell
cmake -S . -B build
```

Build:

```powershell
cmake --build build --config Debug
```

The resulting TSF DLL will be located at:

```text
build\Debug\okkhor_tsf.dll
```

---

## Run Tests

Build the tests:

```powershell
cmake --build build --config Debug
```

Then run:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

The tests cover the platform-independent `EngineHost` interface and its interaction with `okkhor-core`.

---

## Developer Installation

After building the project, the local DLL can be registered with:

```powershell
.\scripts\install.ps1 -Dll build\Debug\okkhor_tsf.dll
```

This is useful when developing the TSF implementation because you can build and immediately register the newly compiled DLL.

For the normal end-user installation, use the online installer instead.

---

# Architecture

Okkhor Windows consists of several layers:

```text
┌─────────────────────────────────────┐
│            Windows Apps             │
└──────────────────┬──────────────────┘
                   │
                   ▼
┌─────────────────────────────────────┐
│      Windows Text Services          │
│            Framework                │
└──────────────────┬──────────────────┘
                   │
                   ▼
┌─────────────────────────────────────┐
│        Okkhor Text Service          │
│                                     │
│  Keyboard events / composition /    │
│  TSF communication                  │
└──────────────────┬──────────────────┘
                   │
                   ▼
┌─────────────────────────────────────┐
│            EngineHost               │
│                                     │
│  Windows ↔ Okkhor Core boundary     │
└──────────────────┬──────────────────┘
                   │
                   ▼
┌─────────────────────────────────────┐
│           okkhor-core               │
│                                     │
│  Tokenization / rules / parsing /   │
│  transliteration / rendering        │
└─────────────────────────────────────┘
```

The Windows project is responsible for Windows integration.

The core project is responsible for transliteration.

---

# Troubleshooting

## Okkhor Does Not Appear in the Keyboard List

Make sure that:

1. Installation completed successfully.
2. Okkhor was registered with administrator privileges.
3. You are using 64-bit Windows.
4. You are looking under the Bangla keyboard settings.

Try restarting Windows if the keyboard does not immediately appear.

---

## The Installer Says That the DLL Is Locked

Close applications that may currently be using the Okkhor TSF.

The installer normally stops the relevant Windows components automatically before registering the DLL.

If necessary, restart Windows and run the installer again.

---

## Okkhor Appears but Does Not Produce Bangla

Make sure **Okkhor Phonetic** is selected as the active keyboard.

Also verify that you are typing Latin input into an application that supports Windows text services.

---

## The PowerShell Installer Does Not Run

If PowerShell reports an execution-policy restriction, you can run the installer explicitly with:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\install.ps1
```

For the online installer, use the command documented in the **Recommended Installation** section.

---

# Security

The PowerShell installation command downloads and executes an installer script:

```powershell
irm https://raw.githubusercontent.com/okkhor-bn/okkhor-windows/main/scripts/install.ps1 | iex
```

Only run this command if you trust the Okkhor repository.

For users who prefer not to execute a remote PowerShell script, download a release manually from the project's GitHub Releases page and install it using the provided installer.

---

# Related Projects

- **[okkhor-core](https://github.com/okkhor-bn/okkhor-core)** — Okkhor's platform-independent transliteration engine.
- **[okkhor-cli](https://github.com/okkhor-bn/okkhor-cli)** — Command-line interface for Okkhor.
- **Okkhor Android** — Android keyboard integration.

---

# Contributing

Contributions, bug reports, feature requests, and improvements are welcome.

Before contributing, please make sure the project builds successfully and that existing tests pass.

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

For development and implementation details, see the project's contribution documentation.

---

# License

See the repository license for licensing information.
