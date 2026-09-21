// src/core/engine_host.hpp
//
// The adapter that owns the okkhor-core engine.
//
// This file and its .cpp are deliberately free of Windows headers: they are the
// seam between the platform frontend and the platform-independent core, and
// they are compiled and tested on any platform (see tests/engine_host_tests.cpp).
//
// Responsibilities:
//   * own the okkhor::Engine instance and its lifetime
//   * turn core exceptions into return codes, because a text service must never
//     let an exception escape into a COM method or a host application
//
// It contains no transliteration logic of its own. Every orthographic decision
// belongs to okkhor-core.
#pragma once

#include <memory>
#include <string>

namespace okkhor {
class Engine;
}

namespace okkhor_windows {

class EngineHost {
public:
    EngineHost();
    ~EngineHost();

    EngineHost(const EngineHost&) = delete;
    EngineHost& operator=(const EngineHost&) = delete;
    EngineHost(EngineHost&&) noexcept;
    EngineHost& operator=(EngineHost&&) noexcept;

    // Loads the mapping data and the contextual rules from `data_dir`.
    // Returns false and fills `error` on failure; the host stays unloaded.
    bool Load(const std::string& data_dir, std::string* error);

    void Unload();
    bool ready() const { return engine_ != nullptr; }

    // Latin (UTF-8) -> Bangla (UTF-8), via okkhor::Engine::transliterate.
    //
    // Returns false if the engine is not loaded or the core threw. On failure
    // `out` is left untouched so the caller can keep showing the previous
    // rendering rather than pushing malformed text into the document.
    bool Transliterate(const std::string& latin_utf8, std::string* out) const;

    // The number of phonetic tokens the core reads `latin_utf8` as. Used only
    // to decide how much of the Latin buffer a Backspace removes; the frontend
    // never interprets what the tokens mean.
    bool TokenCount(const std::string& latin_utf8, std::size_t* out) const;

    // Length in bytes of the Latin prefix that remains after dropping the last
    // phonetic token. Backspace edits the Latin buffer at this boundary, which
    // is why the frontend never has to cut UTF-8 bytes off rendered Bangla.
    bool LengthWithoutLastToken(const std::string& latin_utf8, std::size_t* out) const;

private:
    std::unique_ptr<okkhor::Engine> engine_;
};

}  // namespace okkhor_windows
