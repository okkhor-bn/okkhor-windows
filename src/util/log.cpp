// src/util/log.cpp
#include "util/log.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>

#include <cstdio>
#include <mutex>

#include "windows/unicode.hpp"

namespace okkhor_windows::log
{
    namespace
    {

        std::mutex g_mutex;
        std::wstring g_path;
        bool g_initialized = false;

        std::wstring DefaultLogPath()
        {
            PWSTR local_appdata = nullptr;
            if (FAILED(::SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local_appdata)))
                return {};
            std::wstring dir(local_appdata);
            ::CoTaskMemFree(local_appdata);

            dir += L"\\Okkhor";
            ::CreateDirectoryW(dir.c_str(), nullptr);
            return dir + L"\\okkhor-windows.log";
        }

        const char *LevelName(Level level)
        {
            switch (level)
            {
            case Level::Debug:
                return "DEBUG";
            case Level::Info:
                return "INFO ";
            case Level::Warn:
                return "WARN ";
            case Level::Error:
                return "ERROR";
            }
            return "?????";
        }

        void WriteLine(Level level, const std::string &message)
        {
#ifndef OKKHOR_WINDOWS_LOGGING
            (void)level;
            (void)message;
#else
            std::lock_guard<std::mutex> guard(g_mutex);
            if (!g_initialized)
                return;
            if (g_path.empty())
                return;

            FILE *file = nullptr;
            if (::_wfopen_s(&file, g_path.c_str(), L"a, ccs=UTF-8") != 0 || !file)
                return;

            SYSTEMTIME now{};
            ::GetLocalTime(&now);
            std::fwprintf(file, L"%04u-%02u-%02u %02u:%02u:%02u.%03u [%5u] %S %S\n", now.wYear,
                          now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
                          ::GetCurrentThreadId(), LevelName(level), message.c_str());
            std::fclose(file);
#endif
        }

    } // namespace

    void Initialize()
    {
        std::lock_guard<std::mutex> guard(g_mutex);
        if (g_initialized)
            return;
        g_path = DefaultLogPath();
        g_initialized = true;
    }

    void Shutdown()
    {
        std::lock_guard<std::mutex> guard(g_mutex);
        g_initialized = false;
        g_path.clear();
    }

    void Write(Level level, const std::string &message) { WriteLine(level, message); }

    void Write(Level level, const std::wstring &message)
    {
        WriteLine(level, WideToUtf8(message));
    }

    void WriteComposition(const char *label, const std::string &utf8_text)
    {
        WriteLine(Level::Debug, std::string(label) + ": " + utf8_text);
    }

} // namespace okkhor_windows::log
