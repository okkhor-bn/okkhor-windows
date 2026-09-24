#pragma once

#include <okkhor/okkhor.hpp>

#include <cstddef>
#include <string>
#include <string_view>

namespace okkhor_windows
{

    class EngineHost
    {
    public:
        EngineHost();
        ~EngineHost();

        EngineHost(const EngineHost &) = delete;
        EngineHost &operator=(const EngineHost &) = delete;

        EngineHost(EngineHost &&) noexcept;
        EngineHost &operator=(EngineHost &&) noexcept;

        bool Transliterate(
            std::string_view latin_utf8,
            std::string *out) const;

    private:
        okkhor::Engine engine_;
    };

} // namespace okkhor_windows