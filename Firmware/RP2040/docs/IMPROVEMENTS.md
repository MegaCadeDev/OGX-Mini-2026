# Firmware Improvements

Improvements and fixes applied to the OGX-Mini RP2040 firmware in this project.

**Version:** From **v1.0.0a3** the version was bumped to **v1.0.0a4** to reflect Wii U controller fixes, Gamecube USB mode, PS3 driver fixes, latency improvements, and Xbox 360 (XInput) support (see below).

---

## Xbox 360 (XInput) support

**Goal:** Use the adapter in XInput mode on Xbox 360 with Bluetooth controllers (e.g. PS5, Xbox One). The 360 requires XSM3 authentication and specific USB descriptors.

**References:** [joypad-os](https://github.com/joypad-ai/joypad-os) XInput implementation; [libxsm3](https://github.com/InvoxiPlayGames/libxsm3).

### Changes

1. **Descriptors**
   - Device and configuration descriptors aligned with joypad-os (153-byte config, 4 interfaces, bConfigurationValue 1, bMaxPower 0xFA).
   - Configuration callback returns `nullptr` for `index != 0` (single config).
   - String descriptor index 4 (XSM3 security) uses a 96-character buffer and the full string (no 31-char truncation).

2. **XSM3 authentication**
   - XSM3 state is initialized at driver init (not in the 0x81 callback).
   - Challenge init (0x82) and verify (0x87) data are stored when received; crypto (`xsm3_do_challenge_init` / `xsm3_do_challenge_verify`) runs in the main loop (`process()`), not in the USB callback.
   - 0x83 responses: 46 bytes for init, 22 bytes for verify.
   - 0x86 state: 1 = processing, 2 = response ready.

3. **USB on Pico 2 W**
   - USB is initialized before Core1 (Bluetooth) so the 360 can enumerate and run XSM3 even if BT firmware is still loading.

4. **Control handling**
   - Vendor and class control requests are forwarded to the active driver so XSM3 traffic reaches the XInput handler.

Descriptors and XSM3 flow are aligned with [joypad-os](https://github.com/joypad-ai/joypad-os); a local comparison doc may be kept out of the repo for reference.

---

## PS3 mode — input delays and stuck inputs

**Source:** Fixes from [OGX-Mini-Plus](https://github.com/guimaraf/OGX-Mini-Plus) (v1.1.1) — *“PS3 Driver Fixes - Fixed input delays and stuck inputs.”*

**File:** `src/USBDevice/DeviceDriver/PS3/PS3.cpp`

### Changes

1. **L2/R2 axis values**
   - `report_in_.l2_axis` and `report_in_.r2_axis` are now set from `gp_in.trigger_l` and `gp_in.trigger_r`.
   - Previously left at zero, which could cause stuck or incorrect trigger behaviour on PS3. Filling these is required for many PS3 games.

2. **Joystick centre and deadzone**
   - Replaced generic `Scale::int16_to_uint8()` (centre 128) with PS3-specific mapping:
     - Centre **0x7F (127)** and ~5% deadzone (~1640 on a ±32768 scale).
     - In deadzone, sticks report exactly 0x7F so the console does not see drift.
     - Outside deadzone, values are scaled into 0–254 so 127 remains the logical centre.
   - Avoids stick drift and “stuck” stick behaviour caused by wrong centre or jitter.

3. **D-pad in analog mode**
   - When `gamepad.analog_enabled()` is true, D-pad axes (`up_axis`, `down_axis`, `left_axis`, `right_axis`) are now derived from the **digital** D-pad bits instead of `gp_in.analog[ANALOG_OFF_*]`.
   - Prevents noisy analog D-pad values from causing stuck or wrong D-pad input on PS3.

4. **Face button axes (digital / non-analog branch)**
   - Corrected mapping so that:
     - `circle_axis` = BUTTON_B (was BUTTON_X)
     - `cross_axis` = BUTTON_A (was BUTTON_B)
     - `square_axis` = BUTTON_X (was BUTTON_A)
   - Circle / Cross / Square now match the intended face buttons.

---

## Latency reduction

**Goal:** Reduce input-to-output latency in the device (Core0) main loop, especially for XInput with Bluetooth controllers (PS5, Xbox One).

### Main loop delay

- **Before:** The device loop used `sleep_ms(1)` every iteration, then a configurable `MAIN_LOOP_DELAY_US` (default 250 µs).
- **After:**
  - **Default: 0 µs** — no added delay; loop runs as fast as possible for minimum latency (low-latency default).
  - **250+ µs** — set via CMake (e.g. `-DMAIN_LOOP_DELAY_US=250`) to reduce CPU use if desired.

**Files changed:**

- `src/Board/Config.h` — `MAIN_LOOP_DELAY_US` default **0**; override via CMake.
- `src/OGXMini/Board/Standard.cpp`, `PicoW.cpp`, `Four_Channel_I2C.cpp` — use `sleep_us(MAIN_LOOP_DELAY_US)` when `> 0`.
- `CMakeLists.txt` — `MAIN_LOOP_DELAY_US` cache variable (default 0).

### XInput: always send latest state

In XInput mode, the adapter no longer sends a report only when `gamepad.new_pad_in()` is true. It **always** reads the current gamepad state and sends whenever the USB IN endpoint is free. That way the host (e.g. 360) gets updates at its poll interval (4 ms) with the freshest state, instead of being limited by how often the BT stack sets “new input.” Reduces perceived latency with PS5/Xbox One over Bluetooth.

**File:** `src/USBDevice/DeviceDriver/XInput/XInput.cpp` — `process()` always builds and sends the report; no gate on `new_pad_in()`.

### How to use

| Goal | Setting |
|------|--------|
| Low latency (default) | Use default `MAIN_LOOP_DELAY_US=0`. |
| Lower CPU use | Configure with e.g. `-DMAIN_LOOP_DELAY_US=250`. |

### Notes

- Core1 (Bluetooth / gamepad) runs with no sleep in its loop.
- USB full-speed poll interval (e.g. 4 ms for XInput) still applies; the improvement is that each report carries the **latest** input and the main loop adds no extra delay by default.
- Bluetooth adds latency; the changes above minimize the adapter’s contribution.

---

## Summary

| Area | Improvement |
|------|-------------|
| **XInput (360)** | XSM3 authentication and descriptors aligned with joypad-os; adapter works on Xbox 360 with BT controllers (PS5, Xbox One). |
| **PS3** | Stuck inputs and input delays addressed via L2/R2 axes, joystick centre/deadzone, D-pad handling, and face button mapping. |
| **Latency** | Main loop delay default **0 µs** (low latency); XInput always sends latest state when USB is ready for lower latency with PS5/Xbox One over BT. |
