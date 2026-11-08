#ifndef _OGXM_SWITCH_HD_RUMBLE_H_
#define _OGXM_SWITCH_HD_RUMBLE_H_

#include <cstdint>

/*  Decoding of Switch HD rumble motor blocks sent by the host.
 *
 *  A motor block is a little-endian 32-bit word. Its top two bits give the packet type, and the
 *  rest carries 1 to 3 samples for the high and low bands:
 *    - 7-bit values set an amplitude (or frequency) outright. The common "one 7-bit sample"
 *      form is the one from dekuNukem's rumble_data_table.md, and 00 01 40 40 is its neutral.
 *    - 5-bit values are commands relative to the current state (reset to silence, substitute
 *      a level, step up/down, or leave it). So the pad keeps state between packets.
 *  Steam, for instance, sends UI "ticks" as 00 00 05 c0: three 5-bit samples, silence -> a
 *  4% blip -> silence. Read as the plain form, that was a strong rumble that never stopped
 *  (the host sends no neutral afterwards, since the packet already ends silent).
 *
 *  Only amplitudes are tracked: the Bluetooth pads are driven with a single intensity.
 */
namespace hd_rumble {

    class Decoder {
    public:
        // Apply one motor block and return the intensity 0-255 it leaves the motor at
        // (the last sample: a blip in the middle of a packet is shorter than we can play).
        uint8_t apply(const uint8_t block[4]);

    private:
        // log2 of the linear amplitude, -8 (silent) .. 0.
        float low_ = -8.0f;
        float high_ = -8.0f;
    };

} // namespace hd_rumble

#endif // _OGXM_SWITCH_HD_RUMBLE_H_
