// src/core/engine_host.cpp
#include "core/engine_host.hpp"

#include <exception>
#include <vector>

#include "mapping.hpp"
#include "okkhor.hpp"
#include "rules.hpp"
#include "tokenizer.hpp"

namespace okkhor_windows {

EngineHost::EngineHost() = default;
EngineHost::~EngineHost() = default;
EngineHost::EngineHost(EngineHost&&) noexcept = default;
EngineHost& EngineHost::operator=(EngineHost&&) noexcept = default;

bool EngineHost::Load(const std::string& data_dir, std::string* error) {
    try {
        // NOTE: this repeats what okkhor::Engine::from_data_dir does, on
        // purpose, and it is the only place in this project that touches core
        // construction.
        //
        // okkhor::Renderer stores a pointer to the Mapping it was constructed
        // from, and okkhor::Engine's renderer_ points at its own mapping_
        // member. An Engine is therefore safe to construct but NOT safe to move
        // or copy: a moved Engine's renderer would still point at the mapping
        // of the moved-from object. `from_data_dir` returns a prvalue, so
        //     okkhor::Engine e = okkhor::Engine::from_data_dir(dir);
        // is fine (guaranteed elision), but
        //     auto p = std::make_unique<okkhor::Engine>(Engine::from_data_dir(dir));
        // would move and leave a dangling renderer. Building the Engine
        // in place through its two-argument constructor avoids the move
        // entirely, and needs no change to the core.
        okkhor::Mapping mapping = okkhor::Mapping::load(data_dir);

        okkhor::RuleEngine rules;
        const std::string rules_path =
            data_dir.empty() ? std::string("rules.json") : data_dir + "/rules.json";
        rules.load_file(rules_path, mapping);

        auto engine = std::make_unique<okkhor::Engine>(std::move(mapping), std::move(rules));
        engine_ = std::move(engine);  // moving the unique_ptr, never the Engine
        return true;
    } catch (const std::exception& e) {
        if (error) *error = e.what();
        engine_.reset();
        return false;
    } catch (...) {
        if (error) *error = "unknown error while loading okkhor-core data";
        engine_.reset();
        return false;
    }
}

void EngineHost::Unload() { engine_.reset(); }

bool EngineHost::Transliterate(const std::string& latin_utf8, std::string* out) const {
    if (!engine_ || !out) return false;
    try {
        *out = engine_->transliterate(latin_utf8);
        return true;
    } catch (...) {
        return false;  // keep whatever the caller already had
    }
}

bool EngineHost::TokenCount(const std::string& latin_utf8, std::size_t* out) const {
    if (!engine_ || !out) return false;
    try {
        *out = engine_->tokenize_input(latin_utf8).size();
        return true;
    } catch (...) {
        return false;
    }
}

bool EngineHost::LengthWithoutLastToken(const std::string& latin_utf8, std::size_t* out) const {
    if (!engine_ || !out) return false;
    if (latin_utf8.empty()) {
        *out = 0;
        return true;
    }
    try {
        const std::vector<okkhor::Token> tokens = engine_->tokenize_input(latin_utf8);
        if (tokens.empty()) {
            *out = 0;
            return true;
        }
        // Tokens carry the exact Latin text they consumed, so the sum of all
        // but the last is the surviving prefix. No byte-level guessing.
        std::size_t consumed = 0;
        for (std::size_t i = 0; i + 1 < tokens.size(); ++i) consumed += tokens[i].latin.size();
        if (consumed > latin_utf8.size()) consumed = latin_utf8.size();
        *out = consumed;
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace okkhor_windows
