#include "UserSettings/DongleSettings.h"

#ifndef OGXM_JOYCON_PAIR_IMU_RIGHT
#define OGXM_JOYCON_PAIR_IMU_RIGHT 1
#endif
#ifndef OGXM_JOYCON_PAIR_HORIZONTAL
#define OGXM_JOYCON_PAIR_HORIZONTAL 0
#endif
#ifndef OGXM_JOYCON_SOLO_HORIZONTAL
#define OGXM_JOYCON_SOLO_HORIZONTAL 1
#endif

namespace dongle_settings {

namespace {

void set_default(Settings& s, Option option, int value)
{
    s.option[static_cast<uint8_t>(option)] = value ? 1 : 0;
}

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
    set_default(s, Option::JoyConPairImuRight, OGXM_JOYCON_PAIR_IMU_RIGHT);
    set_default(s, Option::JoyConPairHorizontal, OGXM_JOYCON_PAIR_HORIZONTAL);
    set_default(s, Option::JoyConSoloHorizontal, OGXM_JOYCON_SOLO_HORIZONTAL);
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
