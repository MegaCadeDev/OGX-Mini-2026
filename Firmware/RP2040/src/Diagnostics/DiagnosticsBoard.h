#ifndef _OGXM_DIAGNOSTICS_BOARD_H_
#define _OGXM_DIAGNOSTICS_BOARD_H_

#include "USBDevice/DeviceDriver/DeviceDriverTypes.h"

namespace diag {

    const char* driver_name(DeviceDriverType type);
    // Board info + "boot" event; call once the output mode is known (after flash init).
    void board_boot();

} // namespace diag

#endif // _OGXM_DIAGNOSTICS_BOARD_H_
