#include <cmath>

#include "USBDevice/DeviceDriver/Switch/HdRumble.h"

namespace hd_rumble {

namespace {

    constexpr float kSilent = -8.0f;
    constexpr float kSilentBelow = -7.9375f;
    // Amplitude of 7-bit code 100 (dekuNukem's 1.003, the documented safe maximum) = full scale.
    constexpr float kFullScale = 100 * 0.03125f - 3.96875f;

    uint32_t bits(uint32_t v, int shift, int count)
    {
        return (v >> shift) & ((1u << count) - 1u);
    }

    float amp_7bit(uint32_t code)
    {
        if (code == 0)
            return kSilent;
        if (code < 16)
            return 0.25f * code - 7.75f;
        if (code < 32)
            return 0.0625f * code - 4.9375f;
        return 0.03125f * code - 3.96875f;
    }

    // Amplitude part of a 5-bit command (the frequency part is not tracked).
    float amp_5bit(uint32_t code, float current)
    {
        float step;
        if (code == 0)
            return kSilent;                     // reset to default
        if (code <= 11)
            return -0.5f * (code - 1);          // substitute 0, -0.5 .. -5
        if (code <= 16)
            return current;                     // frequency-only commands
        if (code <= 19)
            step = 0.125f;
        else if (code <= 22)
            step = 0.03125f;
        else if (code <= 25)
            return current;
        else if (code <= 28)
            step = -0.03125f;
        else
            step = -0.125f;
        const float v = current + step;
        return v < kSilent ? kSilent : (v > 0.0f ? 0.0f : v);
    }

    uint8_t to_intensity(float log2_amp)
    {
        if (log2_amp < kSilentBelow)
            return 0;
        const float v = std::exp2(log2_amp - kFullScale) * 255.0f + 0.5f;
        return v >= 255.0f ? 255 : static_cast<uint8_t>(v);
    }

} // namespace

uint8_t Decoder::apply(const uint8_t block[4])
{
    const uint32_t v = static_cast<uint32_t>(block[0]) | (static_cast<uint32_t>(block[1]) << 8) |
                       (static_cast<uint32_t>(block[2]) << 16) | (static_cast<uint32_t>(block[3]) << 24);

    // Apply one 5-bit sample (low and high band commands).
    auto step = [this](uint32_t lo, uint32_t hi) {
        low_ = amp_5bit(lo, low_);
        high_ = amp_5bit(hi, high_);
    };

    switch (v >> 30)
    {
        case 0:  // no samples: state unchanged
            break;
        case 1:
            if (bits(v, 0, 20) == 0)
            {   // one 5-bit sample
                step(bits(v, 25, 5), bits(v, 20, 5));
            }
            else if (bits(v, 0, 2) == 0)
            {   // one 7-bit sample (the common form)
                high_ = amp_7bit(bits(v, 9, 7));
                low_ = amp_7bit(bits(v, 23, 7));
            }
            else
            {   // three samples: one 7-bit value (bit 0: high band, bit 2: frequency), two 5-bit
                if (!bits(v, 2, 1))
                    (bits(v, 0, 1) ? high_ : low_) = amp_7bit(bits(v, 23, 7));
                step(bits(v, 18, 5), bits(v, 13, 5));
                step(bits(v, 8, 5), bits(v, 3, 5));
            }
            break;
        case 2:
            if (bits(v, 0, 10) == 0)
            {   // two 5-bit samples
                step(bits(v, 25, 5), bits(v, 20, 5));
                step(bits(v, 15, 5), bits(v, 10, 5));
            }
            else
            {   // two samples: 7-bit amplitude for one band + 5-bit for the other, then 5-bit
                const float am = amp_7bit(bits(v, 23, 7));
                const uint32_t other = bits(v, 18, 5);
                if (bits(v, 0, 1))
                {
                    high_ = am;
                    low_ = amp_5bit(other, low_);
                }
                else
                {
                    low_ = am;
                    high_ = amp_5bit(other, high_);
                }
                step(bits(v, 13, 5), bits(v, 8, 5));
            }
            break;
        default:  // three 5-bit samples
            step(bits(v, 25, 5), bits(v, 20, 5));
            step(bits(v, 15, 5), bits(v, 10, 5));
            step(bits(v, 5, 5), bits(v, 0, 5));
            break;
    }

    const uint8_t lo = to_intensity(low_);
    const uint8_t hi = to_intensity(high_);
    return lo > hi ? lo : hi;
}

} // namespace hd_rumble
