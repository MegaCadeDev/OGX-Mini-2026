#include "UserSettings/DongleSettings.h"

namespace dongle_settings {

namespace {

Settings& current()
{
    static Settings settings = defaults();
    return settings;
}

} // namespace

Settings defaults()
{
    Settings s{};
    s.version = kVersion;
    return s;
}

bool decode(const uint8_t* data, size_t len, Settings& out)
{
    if (data == nullptr || len < sizeof(Settings) || data[0] != kVersion)
        return false;
    Settings s{};
    s.version = kVersion;
    for (size_t i = 0; i < kOptionCount; ++i)
        s.option[i] = data[1 + i] ? 1 : 0;
    out = s;
    return true;
}

const Settings& get()
{
    return current();
}

void set(const Settings& settings)
{
    current() = settings;
}

} // namespace dongle_settings
