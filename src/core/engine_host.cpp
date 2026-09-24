#include "core/engine_host.hpp"

#include <exception>

namespace okkhor_windows
{

    EngineHost::EngineHost() = default;

    EngineHost::~EngineHost() = default;

    EngineHost::EngineHost(EngineHost &&) noexcept = default;

    EngineHost &EngineHost::operator=(EngineHost &&) noexcept = default;

    bool EngineHost::Transliterate(
        std::string_view latin_utf8,
        std::string *out) const
    {
        if (!out)
        {
            return false;
        }

        try
        {
            *out =
                engine_.transliterate_latin_to_bangla(
                    latin_utf8);

            return true;
        }
        catch (const std::exception &)
        {
            return false;
        }
        catch (...)
        {
            return false;
        }
    }

} // namespace okkhor_windows