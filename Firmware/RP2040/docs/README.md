# RP2040 firmware docs

See the [project README](../../README.md) for overview, supported platforms, build, and configuration.

## Firmware docs (this folder)

| Document | Description |
|----------|-------------|
| [**Controller_Mappings.md**](Controller_Mappings.md) | **Master reference:** PadIn layout, input → PadIn (BT/USB), and PadIn → every output mode (XInput, PS3, PS4, Switch, STEAM, OG Xbox, Wii U, GPIO, Wii output). |
| [Wii_Mode_Guide.md](Wii_Mode_Guide.md) | Wii mode (build-option only): No Extension / Nunchuk / Classic, USB host, sync and auto-connect, button mapping, supported controllers. |
| [PICO2W_WII_USB_SETUP.md](PICO2W_WII_USB_SETUP.md) | Pico 2 W / Pico W: USB host wiring (PIO USB), pins, build, troubleshooting for Wii mode. |
| [GPIO_Output_Pinout_and_Mappings.md](GPIO_Output_Pinout_and_Mappings.md) | GPIO pin-outs and default button/stick mappings for PS1/PS2, Dreamcast, GameCube, and N64 output (and input) modes. |
| [Dreamcast_Port.md](Dreamcast_Port.md) | Dreamcast (Maple Bus) port status and notes for completing the port from DreamPicoPort (stub only in firmware for now). |
| [IMPROVEMENTS.md](IMPROVEMENTS.md) | Firmware improvements and **Future planned** (e.g. Xbox One/Series wireless dongle `045e:02e6` / `02fe`); **STEAM mode**, **PS3/PS4 motion**, **XInput stock stick feel (#38)**, **Switch HD rumble**, **Switch 2 anti-dz (#64)**, Pico W Bluetooth, Switch 2 / Joy-Con merge, **Steam Controller 2026 (Triton) BLE**, USB resume pairing, PIO USB unplug detection. |
