#ifndef _OGXM_SWITCH_IMU_H_
#define _OGXM_SWITCH_IMU_H_

#include <cstdint>

#include "Gamepad/Gamepad.h"

/*  Motion for the emulated Switch Pro Controller.
 *
 *  The Switch output reports this factory IMU calibration (SPI 0x6020): zero offsets,
 *  accel scale 0x4000 and gyro scale 0x343B. With it, hosts (console, SDL/Steam, Linux
 *  hid-nintendo) read accel as 4096 counts per g and gyro as 13371/936 counts per deg/s.
 *  This module converts PadIn motion into one such raw sample, in the Pro Controller's
 *  own axes.
 */
namespace switch_imu {

    constexpr int32_t kAccelCountsPerG = 4096;
    constexpr int32_t kGyroScale = 0x343B;        // 13371
    constexpr int32_t kGyroScaleDegPerS = 936;    // counts = dps * kGyroScale / 936

    struct Sample {
        int16_t accel[3];
        int16_t gyro[3];
    };
    static_assert(sizeof(Sample) == 12, "Switch IMU sample is 12 bytes on the wire");

    // Factory IMU calibration returned for SPI address 0x6020 (24 bytes, little endian):
    // accel offset x,y,z | accel scale x,y,z | gyro offset x,y,z | gyro scale x,y,z.
    extern const uint8_t kFactoryCalibration[24];

    // False (and a zeroed sample) when the pad has no usable motion source.
    bool to_switch_sample(const Gamepad::PadIn& in, Sample& out);

} // namespace switch_imu

#endif // _OGXM_SWITCH_IMU_H_
