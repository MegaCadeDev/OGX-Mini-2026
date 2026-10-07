#ifndef _OGXM_SONY_REPORTS_H_
#define _OGXM_SONY_REPORTS_H_

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "USBDevice/DeviceDriver/Sony/SonyImu.h"

/*  Input report fields of the emulated DualSense (STEAM mode), kept free of TinyUSB. Offsets
 *  are within the full report, report ID at [0], as Linux hid-playstation reads it.
 */
namespace sony_reports {

    /* TinyUSB already put the report ID in front when the host asked for a specific one; copy
     * the rest of a full report (ID at [0]). */
    inline size_t copy_for_get_report(uint8_t requested_id, const uint8_t* report, size_t report_len,
                                      uint8_t* buffer, size_t reqlen)
    {
        const size_t skip = requested_id == 0 ? 0 : 1;
        if (report_len < skip)
            return 0;
        const size_t avail = report_len - skip;
        const size_t n = reqlen < avail ? reqlen : avail;
        std::memcpy(buffer, report + skip, n);
        return n;
    }

    /* Two touch points of 4 bytes (DS4 and DualSense share the format). Bit 7 of a point's
     * first byte set = not touching; an all-zero report shows two fingers resting at (0, 0) to
     * hosts that read the first touch report directly (SDL / Steam). */
    inline void put_touch_points(uint8_t* dst, const uint8_t touch_raw[8], bool valid)
    {
        if (valid) {
            std::memcpy(dst, touch_raw, 8);
        } else {
            std::memset(dst, 0, 8);
            dst[0] = 0x80;
            dst[4] = 0x80;
        }
    }

    /* Battery 1..255 (Bluepad32 scale, 0 = unknown) -> 0..10. */
    inline uint8_t battery_level_0_10(uint8_t battery)
    {
        return static_cast<uint8_t>((battery * 10u + 127u) / 255u);
    }

    /* DualSense status byte: level in the low nibble, charging state in the high one (2 = full).
     * Unknown: full. */
    inline uint8_t ds5_status(uint8_t battery)
    {
        return battery == 0 ? 0x2A : battery_level_0_10(battery);
    }

    /* DualSense motion sensor clock, in units of 1/3 us (32 bits). Hosts derive the sample
     * interval from it. */
    inline uint32_t ds5_sensor_timestamp(uint64_t time_us)
    {
        return static_cast<uint32_t>(time_us * 3u);
    }

    /* Motion (input units: 1024 per deg/s, 8192 per g) into int16 LE fields. */
    inline void put_motion(uint8_t* report, int gyro_offset, int accel_offset,
                           const int32_t gyro[3], const int32_t accel[3], const sony_imu::MotionScale& scale)
    {
        for (int i = 0; i < 3; ++i) {
            sony_imu::put_le16(report, gyro_offset + i * 2, sony_imu::scale(gyro[i], scale.gyro_div));
            sony_imu::put_le16(report, accel_offset + i * 2, sony_imu::scale(accel[i], scale.accel_div));
        }
    }

    /* DualSense USB input report 0x01 offsets (PS5::InReport lacks the 4 reserved bytes after the
     * buttons, so it only fits up to them). */
    namespace ds5 {
        constexpr int kSeq = 7;
        constexpr int kButtons2 = 10;
        constexpr int kGyro = 16;
        constexpr int kAccel = 22;
        constexpr int kSensorTimestamp = 28;
        constexpr int kTouchPoints = 33;
        constexpr int kStatus = 53;
    }
    constexpr uint8_t kTouchpadClick = 0x02;   // buttons[2] bit

    /* What a synthesized DualSense report (STEAM mode, any pad but a real DualSense) used to
     * leave at zero: sequence, motion (already in the DS4 playing frame; skipped when absent),
     * sensor clock, touch points and click, battery status. */
    struct Ds5SynthInput {
        uint8_t seq;
        bool has_motion;
        int32_t gyro[3];
        int32_t accel[3];
        uint64_t time_us;
        const uint8_t* touch_raw;   // 8 bytes, or null
        bool touch_valid;
        bool touch_click;
        uint8_t battery;
    };

    inline void ds5_fill_synth(uint8_t* report, const Ds5SynthInput& in)
    {
        report[ds5::kSeq] = in.seq;
        if (in.has_motion)
            put_motion(report, ds5::kGyro, ds5::kAccel, in.gyro, in.accel, sony_imu::kRealUnits);
        const uint32_t ts = ds5_sensor_timestamp(in.time_us);
        std::memcpy(&report[ds5::kSensorTimestamp], &ts, sizeof(ts));
        const bool touch = in.touch_valid && in.touch_raw != nullptr;
        put_touch_points(&report[ds5::kTouchPoints], in.touch_raw, touch);
        if (touch && in.touch_click)
            report[ds5::kButtons2] |= kTouchpadClick;
        report[ds5::kStatus] = ds5_status(in.battery);
    }

} // namespace sony_reports

#endif // _OGXM_SONY_REPORTS_H_
