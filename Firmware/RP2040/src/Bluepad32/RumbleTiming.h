#ifndef _RUMBLE_TIMING_H_
#define _RUMBLE_TIMING_H_

#include <cstdint>

/*  Rumble timing for Switch pads.
 *
 *  Switch pads keep vibrating with the last rumble data they received. The feedback loop
 *  re-sends "rumble for N ms" every kFeedbackPeriodMs while the host requests rumble, and the
 *  parser sends a single "stop" when N ms pass without a refresh. kRumbleDurationMs is longer
 *  than the feedback period, so a long rumble (cutscenes) is not stopped and restarted on every
 *  cycle (it used to be: duration == period, the stop always fired first, doubling the outgoing
 *  traffic).
 *
 *  That single "stop" used to be lost when Bluepad32's 32-slot output queue was full, leaving the
 *  motor on; the byte-stream output queue (patch bluepad32_output_ring_buffer.diff, upstream
 *  ricardoquesada/bluepad32 b6531db) holds about 270 rumble packets.
 */
namespace switch_rumble {

    constexpr uint32_t kFeedbackPeriodMs = 250;
    constexpr uint32_t kRumbleDurationMs = kFeedbackPeriodMs + 100;
    static_assert(kRumbleDurationMs > kFeedbackPeriodMs, "a long rumble must outlive one feedback period");

} // namespace switch_rumble

#endif // _RUMBLE_TIMING_H_
