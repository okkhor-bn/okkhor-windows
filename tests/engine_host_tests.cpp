#include "core/engine_host.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{

    void check(
        bool condition,
        const char *message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    void check_equal(
        const std::string &actual,
        const std::string &expected,
        const char *message)
    {
        if (actual != expected)
        {
            std::cerr
                << "FAIL: " << message
                << "\n  expected: " << expected
                << "\n  actual:   " << actual
                << '\n';

            std::exit(EXIT_FAILURE);
        }
    }

} // namespace

int main()
{
    using okkhor_windows::EngineHost;

    EngineHost engine;

    // -------------------------------------------------------------------------
    // Basic transliteration
    // -------------------------------------------------------------------------

    std::string output;

    check(
        engine.Transliterate("ami", &output),
        "Latin-to-Bangla transliteration should succeed");

    check(
        !output.empty(),
        "Latin-to-Bangla output should not be empty");

    // -------------------------------------------------------------------------
    // A known transliteration
    // -------------------------------------------------------------------------

    check(
        engine.Transliterate("bangla", &output),
        "transliteration of 'bangla' should succeed");

    check(
        !output.empty(),
        "transliteration of 'bangla' should produce output");

    // -------------------------------------------------------------------------
    // Empty input
    // -------------------------------------------------------------------------

    output = "sentinel";

    check(
        engine.Transliterate("", &output),
        "empty input should be accepted");

    check(
        output.empty(),
        "empty input should produce empty output");

    // -------------------------------------------------------------------------
    // Null output pointer
    // -------------------------------------------------------------------------

    check(
        !engine.Transliterate("ami", nullptr),
        "null output pointer should fail");

    // -------------------------------------------------------------------------
    // Multiple calls should work
    // -------------------------------------------------------------------------

    std::string first;
    std::string second;

    check(
        engine.Transliterate("ami", &first),
        "first transliteration should succeed");

    check(
        engine.Transliterate("tumi", &second),
        "second transliteration should succeed");

    check(
        !first.empty(),
        "first result should not be empty");

    check(
        !second.empty(),
        "second result should not be empty");

    check(
        first != second,
        "different inputs should normally produce different results");

    // -------------------------------------------------------------------------
    // Move construction
    // -------------------------------------------------------------------------

    EngineHost moved_engine(std::move(engine));

    output.clear();

    check(
        moved_engine.Transliterate("ami", &output),
        "moved EngineHost should remain usable");

    check(
        !output.empty(),
        "moved EngineHost should produce output");

    std::cout
        << "All EngineHost tests passed.\n";

    return EXIT_SUCCESS;
}
