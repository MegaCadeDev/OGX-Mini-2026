#include "USBDevice/DeviceDriver/Switch/SwitchImu.h"

namespace switch_imu {

const uint8_t kFactoryCalibration[24] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // accel offsets
    0x00, 0x40, 0x00, 0x40, 0x00, 0x40,  // accel scale 0x4000
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // gyro offsets
    0x3B, 0x34, 0x3B, 0x34, 0x3B, 0x34,  // gyro scale 0x343B
};

namespace {

// Bluepad32 / wired DS4 & DualSense motion: 8192 per g and 1024 per deg/s (DS4 axes).
constexpr int32_t kDs4AccelPerG = 8192;
constexpr int32_t kDs4GyroPerDegS = 1024;
// Bluepad32 Switch parser: accel in raw counts, gyro in raw counts * 1000 (Pro axes).
constexpr int32_t kSwitchGyroPrec = 1000;

int16_t clamp16(int64_t v)
{
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return static_cast<int16_t>(v);
}

int64_t div_round(int64_t num, int64_t den)
{
    return (num >= 0) ? (num + den / 2) / den : (num - den / 2) / den;
}

} // namespace

bool to_switch_sample(const Gamepad::PadIn& in, Sample& out)
{
    out = Sample{};
    int32_t a[3] = {in.accel[0], in.accel[1], in.accel[2]};
    int32_t g[3] = {in.gyro[0], in.gyro[1], in.gyro[2]};

    switch (in.motion_source)
    {
        case Gamepad::PadIn::MOTION_SRC_SWITCH_PRO:
        case Gamepad::PadIn::MOTION_SRC_SWITCH_USB:
            for (int i = 0; i < 3; ++i)
            {
                out.accel[i] = clamp16(a[i]);
                out.gyro[i] = clamp16(div_round(g[i], kSwitchGyroPrec));
            }
            return true;

        case Gamepad::PadIn::MOTION_SRC_DS4:
        case Gamepad::PadIn::MOTION_SRC_DS5:
        case Gamepad::PadIn::MOTION_SRC_DS4_USB:
        case Gamepad::PadIn::MOTION_SRC_DS5_USB:
        {
            /* DS4 axes as Bluepad32 delivers them: X right, Y out of the face, Z towards the
             * player. Pro axes: X front, Y left, Z out of the face. So Pro = (-z, -x, y), a
             * proper rotation, applied to accel and gyro alike. Measured on hardware: face up
             * gives +1 g on Pro Z; nose down, roll right and turning left give +gy, +gx, +gz. */
            const int32_t sa[3] = {-a[2], -a[0], a[1]};
            const int32_t sg[3] = {-g[2], -g[0], g[1]};
            for (int i = 0; i < 3; ++i)
            {
                out.accel[i] = clamp16(div_round(static_cast<int64_t>(sa[i]) * kAccelCountsPerG, kDs4AccelPerG));
                out.gyro[i] = clamp16(div_round(static_cast<int64_t>(sg[i]) * kGyroScale,
                                                static_cast<int64_t>(kDs4GyroPerDegS) * kGyroScaleDegPerS));
            }
            return true;
        }

        default:
            return false;  // no motion (or Wii pseudo-gyro, not supported here)
    }
}

} // namespace switch_imu
