#ifndef _OGXM_JOYCON_SETTINGS_H_
#define _OGXM_JOYCON_SETTINGS_H_

#include <cstdint>

/*  Joy-Con motion settings.
 *
 *  Views of the adapter options (UserSettings/DongleSettings: web app, defaults from CMake
 *  OGXM_JOYCON_PAIR_IMU_SIDE, OGXM_JOYCON_PAIR_ORIENTATION, OGXM_JOYCON_SOLO_ORIENTATION).
 *
 *  Joy-Con motion arrives in the Pro Controller axes for a Joy-Con held upright
 *  ("vertical", as each half of a pair is held): X towards the top, Y to the left,
 *  Z out of the face. A single Joy-Con used sideways ("horizontal") is rotated 90
 *  degrees about Z: counter-clockwise for the left one, clockwise for the right one.
 */
namespace joycon_settings {

    enum class Orientation : uint8_t { Vertical = 0, Horizontal = 1 };

    struct Settings {
        bool pair_imu_right;
        Orientation pair_orientation;
        Orientation solo_orientation;
    };

    Settings get();

    // Rotate motion (in place) from the upright Joy-Con frame to the given orientation.
    void apply_orientation(bool left_joycon, Orientation orientation, int32_t accel[3], int32_t gyro[3]);

} // namespace joycon_settings

#endif // _OGXM_JOYCON_SETTINGS_H_
