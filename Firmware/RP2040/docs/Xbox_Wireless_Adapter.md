# Xbox Wireless Adapter (USB dongle) support

OGX-Mini can host the **Xbox Wireless Adapter for Windows, 2nd generation** (model 1790, USB `045E:02FE`) on its USB-A port. Xbox One and Xbox Series controllers then connect to the dongle wirelessly and drive any OGX-Mini output mode.

This is an **opt-in personal build**. It needs a firmware file from Microsoft that this repository cannot redistribute, so the prebuilt release UF2s do not include it.

## Status

| Item | State |
|------|-------|
| Dongle | `045E:02FE` tested. The 1st-generation adapter (`045E:02E6`) uses a different firmware file and is not enabled. |
| Controllers | Xbox One controller (`045E:02DD`) tested: buttons, triggers, sticks, both rumble motors, reconnect after sleep. Guide button handling is shared with wired Xbox One pads and not separately verified. |
| Boards | Tested on `PI_PICO`. Enabled for boards that use the Standard host loop: `PI_PICO`, `PI_PICO2`, `RP2040_ZERO`, `ADAFRUIT_FEATHER`, `RP2350_USB_A`, `RP2350_ZERO`, `RP2040_XIAO`, `OGX_LIVE`. |
| Players | One controller per adapter (`MAX_GAMEPADS=1`). |
| Not supported | Headsets and audio, controller firmware updates, chatpads. |

## Getting the radio firmware

The dongle has no firmware of its own. The host uploads it at every power-up, the same way the Windows and Linux drivers do. The file is `FW_ACC_00U.bin`, 70,620 bytes, SHA-256 `48084d9fa53b9bb04358f3bb127b7495dc8f7bb0b3ca1437bd24ef2b6eabdf66`. The build checks this hash.

Either source works:

- **A Windows PC where the dongle is installed.** The file is in the driver store, in a folder named like `C:\Windows\System32\DriverStore\FileRepository\mt7612us_rl.inf_amd64_<hash>\`. Do not use the 70,008-byte copy from `mt7612us.inf_amd64_<hash>`. That one is for the 1st-generation adapter.
- **Microsoft Update Catalog.** The [xone](https://github.com/dlundqvist/xone) project's `install/firmware.sh` lists the driver package that contains it.

## Building

Add one option to a normal build (see [Building_From_Source.md](Building_From_Source.md)):

```
cmake -S . -B build -G Ninja -DOGXM_BOARD=PI_PICO -DMAX_GAMEPADS=1 -DOGXM_XBOX_DONGLE_FW=/path/to/FW_ACC_00U.bin
cmake --build build
```

CMake converts the file into a C array inside the build directory. Nothing is written to the source tree.

## Using it

1. Plug the dongle into the OGX-Mini's USB-A port. Its LED blinks while the OGX-Mini is waiting for a controller.
2. Turn the controller on. A controller that was paired to this dongle before reconnects on its own. For a new controller, hold the controller's pair button until its light flashes fast. The dongle's own button also starts pairing.
3. The dongle LED goes solid when the controller is connected.

While no controller is connected, the dongle stays in pairing mode, so a controller that wakes from sleep can rejoin without pressing anything.

## How it works

- `tuh_xbox_dongle` is a TinyUSB host class driver that claims the dongle's vendor interface and registers an `XBOX_DONGLE` driver with `HostManager`.
- `dongle_core.c` loads the radio firmware, brings the radio up, handles pairing, association and client loss, and passes each controller's GIP packets on. It is adapted from xone's `transport/dongle.c`.
- `mt76.c`, `mt76.h` and `mt76_defs.h` are xone's MediaTek MT7612U radio driver, built through `kcompat.h`, a small shim for the Linux APIs it uses.
- `XboxDongleHost` reuses `XboxOneHost` for GIP input mapping and sends rumble as GIP `0x09` packets.

## Licensing

The `XboxDongle` directory is derived from xone and is licensed **GPL-2.0-or-later**. A UF2 built with `OGXM_XBOX_DONGLE_FW` is therefore distributed under the GPL. Builds without the option contain none of this code. See the repository `LICENSE`.
