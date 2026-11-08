#include "Bluepad32/JoyConSettings.h"
#include "UserSettings/DongleSettings.h"

namespace joycon_settings {

Settings get()
{
    using dongle_settings::Option;
    return Settings{
        dongle_settings::enabled(Option::JoyConPairImuRight),
        dongle_settings::enabled(Option::JoyConPairHorizontal) ? Orientation::Horizontal : Orientation::Vertical,
        dongle_settings::enabled(Option::JoyConSoloHorizontal) ? Orientation::Horizontal : Orientation::Vertical,
    };
}

void apply_orientation(bool left_joycon, Orientation orientation, int32_t accel[3], int32_t gyro[3])
{
    if (orientation == Orientation::Vertical)
        return;
    /* Sideways, the left Joy-Con's top points left (device X -> virtual left, device Y ->
     * virtual back): (x, y, z) -> (-y, x, z). The right one is the mirror: (y, -x, z). */
    int32_t* vectors[2] = {accel, gyro};
    for (int32_t* v : vectors)
    {
        const int32_t x = v[0];
        const int32_t y = v[1];
        v[0] = left_joycon ? -y : y;
        v[1] = left_joycon ? x : -x;
    }
}

} // namespace joycon_settings
