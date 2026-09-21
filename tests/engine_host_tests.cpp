// tests/engine_host_tests.cpp
//
// Platform-independent test of the core seam. It builds and runs on Windows,
// Linux and macOS, and exists to prove that the frontend links okkhor-core
// correctly and drives it the way the composition model will -- before any TSF
// code is involved.
#include <cstdlib>
#include <iostream>
#include <string>

#include "core/engine_host.hpp"
#include "okkhor.hpp"

namespace {

int failures = 0;
int checks = 0;

void check(const std::string& label, const std::string& got, const std::string& want) {
    ++checks;
    if (got == want) return;
    ++failures;
    std::cout << "FAIL " << label << "\n  expected: \"" << want << "\"\n  actual  : \"" << got
              << "\"\n";
}

void check_true(const std::string& label, bool value) {
    ++checks;
    if (value) return;
    ++failures;
    std::cout << "FAIL " << label << " (expected true)\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::string data_dir = argc > 1 ? argv[1] : okkhor::find_data_dir();

    okkhor_windows::EngineHost host;
    std::string error;
    if (!host.Load(data_dir, &error)) {
        std::cout << "FAIL could not load okkhor-core data from \"" << data_dir << "\": " << error
                  << "\n";
        return 1;
    }
    check_true("engine reports ready", host.ready());

    auto render = [&](const std::string& latin) {
        std::string out;
        check_true("transliterate(" + latin + ") succeeded", host.Transliterate(latin, &out));
        return out;
    };

    // Whatever the core says is correct; these only assert that the frontend is
    // asking the core and not transliterating anything itself.
    check("ami", render("ami"), okkhor::Engine::from_data_dir(data_dir).transliterate("ami"));
    check_true("ami is non-empty", !render("ami").empty());
    check_true("incremental prefixes render", !render("a").empty() && !render("am").empty());

    // Backspace walks the Latin buffer one phonetic token at a time.
    {
        const std::string latin = "kha";
        std::size_t tokens = 0;
        check_true("token count", host.TokenCount(latin, &tokens));
        std::size_t shorter = 0;
        check_true("prefix length", host.LengthWithoutLastToken(latin, &shorter));
        check_true("backspace drops a whole token", shorter < latin.size());
        check("backspace keeps the multi-letter token intact", latin.substr(0, shorter), "kh");

        std::size_t empty_len = 1;
        check_true("empty input", host.LengthWithoutLastToken("", &empty_len));
        check_true("empty input yields 0", empty_len == 0);
    }

    // Failure handling: an unloaded engine reports failure instead of throwing.
    {
        okkhor_windows::EngineHost unloaded;
        std::string out = "untouched";
        check_true("unloaded engine fails cleanly", !unloaded.Transliterate("ami", &out));
        check("unloaded engine leaves the output alone", out, "untouched");
    }

    std::cout << "engine_host_tests: " << (checks - failures) << "/" << checks
              << " checks passed\n";
    return failures == 0 ? 0 : 1;
}
