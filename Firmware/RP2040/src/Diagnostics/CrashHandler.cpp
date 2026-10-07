/* Record a crash for the diagnostics report, then reboot (Diagnostics/Diagnostics.h).
 *
 * Without this, a hard fault or a panic() leaves the adapter frozen until it is unplugged, with
 * nothing to tell what happened (a Release build has no log output). The fault address and
 * registers, or the panic message, go into the RAM that survives the reboot, and the adapter
 * restarts; the next boot reports it as "last_crash" and stores it in flash once.
 *
 * Cortex-M0+ (RP2040) has no fault status registers; Cortex-M33 (RP2350) adds CFSR, HFSR, MMFAR
 * and BFAR. The entry code avoids IT blocks so it assembles for both. */
#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "hardware/watchdog.h"
#include "pico/platform.h"
#include "pico/time.h"

#include "Diagnostics/Diagnostics.h"

namespace {

[[noreturn]] void reboot_now()
{
    watchdog_reboot(0, 0, 0);
    for (;;) {
        tight_loop_contents();
    }
}

} // namespace

extern "C" [[noreturn]] void ogxm_fault_record(const uint32_t* frame)
{
    diag::CrashInfo c{};
    c.kind = 1;
    c.core = static_cast<uint8_t>(get_core_num());
    c.lr = frame[5];
    c.pc = frame[6];
    c.xpsr = frame[7];
#if defined(PICO_RP2350)
    volatile const uint32_t* const scb = reinterpret_cast<volatile const uint32_t*>(0xE000ED00u);
    c.has_fault_regs = 1;
    c.cfsr = scb[0x28 / 4];
    c.hfsr = scb[0x2C / 4];
    c.mmfar = scb[0x34 / 4];
    c.bfar = scb[0x38 / 4];
#endif
    c.uptime_ms = to_ms_since_boot(get_absolute_time());
    diag::crash_record(c);
    reboot_now();
}

/* The stacked frame is on MSP or PSP, by bit 2 of EXC_RETURN in LR. */
extern "C" __attribute__((naked)) void isr_hardfault(void)
{
    __asm volatile(
        "movs r0, #4            \n"
        "mov  r1, lr            \n"
        "tst  r0, r1            \n"
        "beq  1f                \n"
        "mrs  r0, psp           \n"
        "b    2f                \n"
        "1:                     \n"
        "mrs  r0, msp           \n"
        "2:                     \n"
        "ldr  r1, =ogxm_fault_record \n"
        "bx   r1                \n"
        ".ltorg                 \n");
}

/* panic() replacement (PICO_PANIC_FUNCTION, set in CMakeLists.txt): the SDK panics when it runs
 * out of something (e.g. spin locks), which would otherwise freeze a Release build silently. The message
 * says what; the caller's address is not kept (the SDK's panic() stub has pushed it). */
extern "C" [[noreturn]] void ogxm_panic(const char* fmt, ...)
{
    diag::CrashInfo c{};
    c.kind = 2;
    c.core = static_cast<uint8_t>(get_core_num());
    c.uptime_ms = to_ms_since_boot(get_absolute_time());
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        std::vsnprintf(c.message, sizeof(c.message), fmt, ap);
        va_end(ap);
    }
    diag::crash_record(c);
#if defined(CONFIG_OGXM_DEBUG)
    printf("\nPANIC: %s\n", c.message);
#endif
    reboot_now();
}
