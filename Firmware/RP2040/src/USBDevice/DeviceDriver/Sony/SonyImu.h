#ifndef _OGXM_SONY_IMU_H_
#define _OGXM_SONY_IMU_H_

#include <cstdint>

/*  Motion for the emulated DualSense (STEAM mode).
 *
 *  Input motion (Bluepad32 / wired hosts) is 1024 per deg/s and 8192 per g. Reports carry real
 *  controller units, 16 per deg/s and 8192 per g, and the calibration feature report describes
 *  an ideal controller with those units. Hosts that apply the calibration (Linux
 *  hid-playstation, SDL) and hosts that assume real units (Steam) then read the same values:
 *    deg/s = raw * (speed_plus + speed_minus) / (|plus - bias| + |minus - bias|)
 *    g     = (raw - bias) * 2 / (acc_plus - acc_minus)
 */
namespace sony_imu {

    /* A report scaling and the calibration that describes it. */
    struct MotionScale {
        int32_t gyro_div;       // input 1024 per deg/s -> report
        int32_t accel_div;      // input 8192 per g -> report
        int16_t cal_gyro_plus;
        int16_t cal_gyro_speed;
        int16_t cal_accel_plus;
    };

    // Real controller units: 16 per deg/s, 8192 per g ((540 + 540) / (2 * 8640) = 1/16).
    // A real pad reports about speed 540 and gyro +-8700.
    constexpr MotionScale kRealUnits{64, 1, 8640, 540, 8192};

    // Rounded division clamped to int16 (truncation zeroed small Bluetooth samples).
    inline int16_t scale(int32_t v, int32_t div)
    {
        const int64_t d = div > 0 ? div : 1;
        const int64_t q = v >= 0 ? (v + d / 2) / d : (v - d / 2) / d;
        return static_cast<int16_t>(q > 32767 ? 32767 : (q < -32768 ? -32768 : q));
    }

    inline void put_le16(uint8_t* report, int offset, int16_t v)
    {
        report[offset] = static_cast<uint8_t>(v & 0xFF);
        report[offset + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    }

    // Calibration feature body, report ID at [0] (DualSense 0x05, same layout as DS4 USB 0x02):
    // biases at [1..6], gyro pitch+/pitch-/yaw+/yaw-/roll+/roll- at [7..18], speed at [19..22],
    // accel x+/x-/y+/y-/z+/z- at [23..34].
    inline void fill_calibration(uint8_t* report, const MotionScale& scale = kRealUnits)
    {
        for (int axis = 0; axis < 3; ++axis) {
            put_le16(report, 1 + axis * 2, 0);
            put_le16(report, 7 + axis * 4, scale.cal_gyro_plus);
            put_le16(report, 9 + axis * 4, static_cast<int16_t>(-scale.cal_gyro_plus));
            put_le16(report, 23 + axis * 4, scale.cal_accel_plus);
            put_le16(report, 25 + axis * 4, static_cast<int16_t>(-scale.cal_accel_plus));
        }
        put_le16(report, 19, scale.cal_gyro_speed);
        put_le16(report, 21, scale.cal_gyro_speed);
    }

} // namespace sony_imu

#endif // _OGXM_SONY_IMU_H_
