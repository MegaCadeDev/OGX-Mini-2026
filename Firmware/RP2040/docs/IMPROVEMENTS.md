# Firmware Improvements

Improvements and fixes applied to the OGX-Mini RP2040 firmware in this project.

**Version:** From **v1.0.0a3** the version was bumped to **v1.0.0a4** to reflect Wii U controller fixes, Gamecube USB mode, PS3 driver fixes, latency improvements, and Xbox 360 (XInput) support (see below). **v1.0.0.8a+** documents **Pico W / Pico 2 W** work on **DualShock 4 (Classic Bluetooth)** vs **BLE advertising**, **BR inquiry**, and related BT stability (see *Pico W / Pico 2 W — DualShock 4 and Classic Bluetooth* below). **v1.0.0.10a** adds **Nintendo Switch 2** wireless support (**Pro 2** and **Joy-Con 2**) over **BLE**, **Switch 1 Joy-Con L+R merge**, and **Joy-Con dual-half latency fixes** (see *Nintendo Switch 2 — Bluetooth* and *Joy-Con pair merge — latency* below). **v1.0.0.11a** adds **PIO USB host wired connection fixes** — **Switch 1/2 Pro**, **DualShock 3**, and **Xbox 360 wireless receiver** — validated on **Waveshare RP2350-USB-A** (see *PIO USB host — wired connection fixes* below).

**Version 1.0.0.9a — documented here for release notes:**

- **Bluetooth (Pico W / Pico 2 W):** ~**1 second** haptic **connection rumble** when a wireless controller reaches **device ready** (so you can feel that pairing completed). **DualShock 4** uses a **longer start delay** before rumble (same idea as the existing PS4 FF grace window) so early force-feedback does not destabilize the link. **File:** `src/Bluepad32/Bluepad32.cpp` — `ogxm_play_connection_rumble()` from `device_ready_cb`.
- **PS3 mode with the adapter plugged into a Windows PC:** **Host output rumble** forwarded to the Bluetooth pad no longer stays on at idle. Windows DInput often sends a **small non-zero** large-motor byte or a **small-motor byte other than 0/1**; the driver now applies a **deadzone** on the large motor and treats the small motor as on **only when the byte is `1`** (DS3 output semantics). **File:** `src/USBDevice/DeviceDriver/PS3/PS3.cpp` — `new_report_out_` path.
- **Pico W / Pico 2 W — PIO USB wired unplug:** **Reliable disconnect** when the gamepad cable is removed from the adapter’s PIO USB host port (see [§ Pico W / Pico 2 W — PIO USB wired controller unplug detection](#pico-w--pico-2-w--pio-usb-wired-controller-unplug-detection)). **Files:** `src/OGXMini/Board/PicoW.cpp`, `src/USBHost/HostManager.h`.
- **DualShock 3 — automatic Bluetooth pairing over USB (boards with Bluetooth):** When a **PS3 / DualShock 3** controller is used **wired** on the USB host, after the normal USB HID init the firmware sends **HID feature report `0xF5`** with the adapter’s **local BD_ADDR** (same as [Bluepad32’s sixaxispairer](https://bluepad32.readthedocs.io/en/latest/pair_ds3/)), so the user can **unplug USB** and connect with **PS**. If `uni_local_bd_addr` is not ready yet, pairing is **deferred** until the first reports where the address is valid. **File:** `src/USBHost/HostDriver/PS3/PS3.cpp` (guarded by `CONFIG_EN_BLUETOOTH`).

---

## Nintendo Switch 2 — Bluetooth (Pico W / Pico 2 W)

**Goal:** Use **Nintendo Switch 2** controllers as **wireless input** on **Pico W** and **Pico 2 W**, in any USB or GPIO output mode (e.g. PS3, XInput, OG Xbox, Switch Pro emulation).

**Confirmed wireless (BLE):**

| Controller | Product ID | Notes |
|------------|------------|--------|
| **Switch 2 Pro** | **0x2069** | Full gamepad; same 63-byte composed report path as Joy-Con 2. |
| **Joy-Con 2 (L)** | **0x2067** | Solo or **merged pair** with Joy-Con R → one player. |
| **Joy-Con 2 (R)** | **0x2066** | Solo or merged with Joy-Con L (pair L first, then SYNC R). |

**Not yet confirmed:** **NSO GameCube** (**0x2073**) — PID registered; GC-style report path exists in the parser.

Switch 2 controllers use a **proprietary BLE GATT protocol**, not standard HID-over-GATT. They advertise as **“Nintendo Switch”** with manufacturer data and require a **custom pairing / SYNC** command sequence before encrypted input notifications arrive.

**References (protocol reverse engineering — thank you to these projects):**

| Project | Contribution |
|---------|----------------|
| **[Nadeflore/switch2-controllers](https://github.com/Nadeflore/switch2-controllers)** | Foundational Switch 2 **BLE discovery, pairing, and GATT** flow for Joy-Con 2 / Pro 2 on PC. |
| **[TommyWabg/switch2-controllers-windows10-gyro](https://github.com/TommyWabg/switch2-controllers-windows10-gyro)** | Extended fork with **63-byte composed input report** layout (button dword @ offset 4, sticks @ 10/13, IMU @ 48), **init / feature / rumble** command formats, and **Pro Controller** rumble packet structure. |
| **[BlueRetro #1249](https://github.com/darthcloud/BlueRetro/issues/1249)** (darthcloud) | Early community notes on **63-byte reports**, **notify handle 0x000A**, and Switch 2 **button bitfield** layout. |
| **`Switch2ProHost.cpp`** (this repo) | **Wired USB** bit layout for PID **0x2069** — cross-check for face / d-pad / **Home → SYS** mapping on PS3-oriented builds. |

### Changes

1. **New Bluepad32 parser** — `Firmware/external/bluepad32/.../parser/uni_hid_parser_switch2.c` (+ `.h`): GATT service discovery, **SYNC / pair** subcommands, calibration read, **input notify** on handle **0x0A**, **command** write/notify, **Pro** and **Joy-Con** vibration characteristics (keepalive rumble), connection parameter update (~7.5 ms), and teardown on disconnect. Supports PIDs **0x2069** (Pro), **0x2066** (Joy-Con R), **0x2067** (Joy-Con L).

2. **LE advertisement hook** — `uni_bt_le.c` detects Switch 2 manufacturer data and routes connect to the Switch 2 parser instead of generic BLE HID.

3. **Input decode** — **63-byte** notify: little-endian **button dword** at offset **4** (masks verified on hardware). Face, d-pad, ±, L3/R3, shoulders, ZL/ZR digital, **Home** (`0x100000` / `0x1000`), and **Capture** mapped into Bluepad32 `uni_gamepad_t`. Sticks calibrated from factory data; gyro/accel forwarded when present.

4. **Home → PS / Guide** — Home sets `MISC_BUTTON_SYSTEM` → `MAP_BUTTON_SYS` in `Bluepad32.cpp` → PS3 / XInput / etc. **Home latch** (~24 frames) in the parser so Nintendo’s often **single-frame** Home pulse is not dropped by the lock-free Core1→Core0 pad staging buffer before the PS3 **8-frame SYS latch** runs.

5. **Link stability** — Periodic **rumble keepalive** (zero-amplitude vibration write every ~5–8 ms) prevents **HCI disconnect (0x08)** when the pad would otherwise go idle without output reports.

6. **OGX integration** — `Bluepad32.cpp`: rumble routing, motion source `MOTION_SRC_SWITCH_PRO`, stall-watchdog exemption, keepalive timer for Switch 2 BLE devices.

7. **Joy-Con 2 pair merge** — When **Left** and **Right** Joy-Con 2 are both connected, input is **merged into one gamepad** (player slot = lower BT index, typically player 1). Left half: left stick, L/ZL, d-pad; right half: face buttons, right stick, R/ZR. Solo Joy-Con keeps BLE scan on for the partner; disconnecting one half unpairs and the other continues solo.

8. **Switch 1 Joy-Con pair merge** — Original **Joy-Con (L/R)** over **Classic Bluetooth** (PIDs **0x2006** / **0x2007**) use the same **L first, then R** flow in `uni_hid_parser_switch.c`. When paired, each half uses **Pro-style** stick/button layout before merge; solo Joy-Con keeps **BR/EDR inquiry** running for the partner. Requires **two** Bluepad32 device slots (`CONFIG_BLUEPAD32_MAX_DEVICES=2`) even on single-player USB builds.

9. **Joy-Con pair latency (Switch 1 + Switch 2)** — When **both** halves send input at once, merged pairs no longer feel laggy or drop button edges. See [§ Joy-Con pair merge — latency when both halves are active](#joy-con-pair-merge--latency-when-both-halves-are-active) below.

**Pairing (user):** Put the **Pro 2** or **Joy-Con 2** in pairing mode (SYNC / hold the pairing button per Nintendo instructions). The adapter scans and connects; **do not** pair the pad in Windows/macOS Bluetooth settings first.

**Joy-Con 2 as one controller (like Switch):** Pair the **Left** Joy-Con first, then put the **Right** Joy-Con in SYNC mode while the left is still connected. The firmware **merges** both halves into **one player** (gamepad slot **1** / the lower BT slot index). Left stick, d-pad, L/ZL map from the left Joy-Con; face buttons, right stick, R/ZR from the right. If only one Joy-Con is connected, it works solo and BLE scan stays on so you can add the partner.

**Switch 1 Joy-Con (same user flow):** Pair **Left** first (hold **SYNC** on the rail), then **Right** while left stays connected. Uses Classic BT inquiry instead of BLE scan. Solo horizontal Joy-Con layout applies until the pair is formed; merged input uses a full gamepad layout. **Wired USB** for Switch 2–family pads remains in **Switch2ProHost** / **SwitchProHost** — see [Wired_Controllers.md](Wired_Controllers.md).

**Files:** `uni_hid_parser_switch2.c`, `uni_hid_parser_switch2.h`, `uni_hid_parser_switch.c`, `uni_hid_parser_switch.h`, `uni_bt_le.c`, `uni_bt_bredr.c`, `uni_hid_device.c`, `uni_controller_list.h`, `src/Bluepad32/Bluepad32.cpp`.

---

## Joy-Con pair merge — latency when both halves are active

**Problem:** With **Left + Right** Joy-Cons merged into one player (Switch 1 Classic BT or Switch 2 BLE), pressing buttons on **both** halves at the same time could feel **laggy** or miss rapid edges. Switch 1 Pro / solo Joy-Con and Switch 2 Pro were unaffected.

**Root causes:**

1. **Stale merge** — Each half’s report was merged using the partner’s last parsed `controller.gamepad`, which could be **one frame behind** when L and R updated in the same interval.
2. **Dropped Core1→Core0 reports** — Bluetooth HID runs on **Core1**; a **single** lock-free staging slot could be **overwritten** before Core0 drained it when L and R fired back-to-back.
3. **Switch 2 radio / BT-thread load** — Dual BLE links plus **keepalive vibration writes** and **verbose raw-input logging** on button changes competed with inbound notifications on the shared CYW43439 radio.

### Changes

#### Shared (Switch 1 + Switch 2 parsers + Gamepad)

1. **Per-half cached state** — Each Joy-Con slot stores its latest parsed `uni_gamepad_t` in a half cache (`sw1_joycon_half_gp` / `sw2_joycon_half_gp`). On any report from either half, merge reads **both caches** and emits **once** through the **primary** (left) device slot only.
2. **2-slot Bluetooth pad staging** — `Gamepad::set_pad_in_from_bluetooth()` uses a **two-entry ring** instead of one overwrite buffer, so rapid L-then-R reports are less likely to lose a frame before Core0 merges them into the main pad-in queue. **File:** `src/Gamepad/Gamepad.h`.

#### Switch 1 (Classic Bluetooth)

3. **IMU off when paired** — After L+R pair is established, both halves receive **SUBCMD_ENABLE_IMU (0)** so 0x30 reports carry less motion traffic and parsing skips IMU while paired. **File:** `uni_hid_parser_switch.c` — `switch_set_imu_enabled()`, `switch_establish_joycon_pair()`.

#### Switch 2 (BLE)

4. **Deferred merge emit** — When paired, L/R input notifications **update the half cache immediately** but schedule a **0 ms** run-loop timer to merge and call `uni_hid_device_process_controller()` **once** per BT tick if both halves updated in the same loop iteration.
5. **Keepalive traffic reduced** — Removed **keepalive on every input notification** (timer already maintains the link). **Secondary** Joy-Con **stops** its 5 ms keepalive timer when paired; **primary** alternates idle rumble between L and R every **10 ms** (one GATT write per tick). Keepalive is **deferred briefly** while either half is actively reporting. **Bluepad32** skips redundant feedback-path keepalive when the parser timer is already running.
6. **Joy-Con–specific parse** — Joy-Con 2 L/R parse **only their physical stick** (not both sticks + IMU on every 63-byte packet). Raw hex input logging on button change is **disabled** by default (`SW2_DEBUG_RAW_INPUT=0`) so UART work does not stall the BT thread during gameplay.
7. **Connection interval** — **7.5 ms** connection parameter update is requested on **both** halves when the pair is formed.

**Files:** `uni_hid_parser_switch.c`, `uni_hid_parser_switch2.c`, `uni_hid_parser_switch2.h`, `src/Gamepad/Gamepad.h`, `src/Bluepad32/Bluepad32.cpp`.

**User impact:** Paired Joy-Cons (Switch 1 or Switch 2) should feel as responsive when using **both** halves simultaneously as when using one half alone. Pairing flow unchanged: **Left first**, then **Right** (SYNC on the partner while left stays connected).

---

## PIO USB host — wired connection fixes (Waveshare RP2350-USB-A)

**Primary test platform:** **Waveshare RP2350-USB-A** (`OGXM_BOARD=RP2350_USB_A`, build option **7** in `scripts/build.sh`). Firmware uses the **Standard** path: **Core1** runs **`tuh_task()`** plus a **1 ms SOF timer** for PIO USB host.

**Scope:** Changes live in **shared USB host drivers** and **`HostManager`**, not board-specific `#ifdef`s. Any build with **`CONFIG_EN_USB_HOST`** (Pico, Feather, RP2350-Zero, RP2350-USB-A, etc.) gets the same wired-host behavior. **Pico W / Pico 2 W Bluetooth** (Bluepad32) is **unchanged**.

**Goal:** Reliable **wired USB host** input on PIO USB adapters for **Switch Pro 1/2**, **DualShock 3**, and the **Xbox 360 wireless PC receiver** + pad — without starving the PIO stack during init or OUT transfers.

### Shared — HostManager and PIO USB servicing

1. **`pio_usb_host_frame()` on all PIO USB host boards** — Previously gated to **Pico W only**; now runs after **`send_feedback()`** / nested **`tuh_task()`** on **every** `CONFIG_EN_USB_HOST` build so wired IN reports do not die after ~1–2 s when the host sends OUT.
2. **Periodic feedback for init-heavy HID drivers** — **`HostManager::send_feedback()`** now always invokes **`send_feedback()`** for **PS3** and **Switch Pro / Switch 2 Pro** (not only on rumble / `new_pad_out()`), so wired init steps and keepalives run on a timer.
3. **`disconnect_cb()` on unmount** — Drivers get a clean teardown hook when a device is removed (bulk cancel for Switch 2, state reset for 360 wireless, etc.).
4. **XInput instance storage** — **`MAX_INTERFACES = MAX_GAMEPADS + OGXM_TUH_XINPUT_INSTANCES`**; **`OGXM_TUH_XINPUT_INSTANCES = 4`** when `MAX_GAMEPADS < 4` so the **360 wireless receiver’s four ports** each have a TinyUSB slot even on single-player builds.

**Files:** `HostManager.h`, `Board/Config.h`.

### Nintendo Switch Pro — wired USB

**References:**

| Project | Contribution |
|---------|----------------|
| **[Chromium `device/gamepad`](https://chromium.googlesource.com/chromium/src/+/main/device/gamepad/)** | **Switch 1 Pro wired USB** init: report **0x80** subcommands (MAC, handshake, baud, disable timeout), **0x81** ack handling, **0x12** output + subcommands for LED / mode / IMU. |
| **`Tools/controller_capture/switch2_usb_init.py`** (this repo) | Captured **Switch 2–family bulk OUT** packet sequence used on PC before HID reports work. |
| **[HandHeldLegend/procon2tool](https://github.com/HandHeldLegend/procon2tool)** | **ProCon 2 enabler** — same bulk bring-up pattern as the capture tool; cross-check for **17-packet** interface **1** sequence. |
| **Bluepad32 `uni_hid_parser_switch.c`** | **Switch 1** button bit layout (`Buttons0/1/2`) for wired report decode validation. |
| **`Switch2ProHost.cpp`** (v1.0.0.9a) | **Switch 2 Pro wired** digitals from report **0x09** — extended here with bulk-only bring-up (no post-bulk Switch 1 init). |

### Switch 1 Pro (PID 0x2009) — wired USB init

1. **Wired USB state machine** — `InitState` phases: **USB_MAC** → **USB_HANDSHAKE1** → **USB_BAUD** → **USB_HANDSHAKE2** → **USB_NO_TIMEOUT** → **LED** → **LED_HOME** → **FULL_REPORT** → **IMU** → **DONE**.
2. **0x80 path** — OUT on report ID **`SwitchPro::REPORT_ID_USB_OUT` (0x80)** with subcommands **`USB_SUB_MAC`**, **`USB_SUB_HANDSHAKE`**, **`USB_SUB_BAUD`**, **`USB_SUB_DISABLE_TIMEOUT` (0x04)**; advance on matching **0x81** IN ack bytes.
3. **0x12 + 0x33 probe** — After disable-timeout, send **`CMD::AND_RUMBLE` (0x12)** with subcommand **`USB_PROBE` (0x33)**; on **0x21** subcommand ack, continue to player LED and **full report mode** subcommands.
4. **PIO-friendly OUT** — **`try_hid_out_id()`** checks **`tuh_hid_send_ready()`**; **`send_feedback()`** calls **`pio_usb_host_frame()`** + **`tuh_task()`** during init so Core1 keeps the PIO host alive.
5. **Per-IN advancement** — Init steps also run from **`process_report()`** on each IN packet, not only from the feedback timer.
6. **Timeout fallback** — If **USB_NO_TIMEOUT** ack is slow, **8 retries** in **`send_feedback()`** force advance to **LED** so init does not hang indefinitely.
7. **Input mapping fixes** — D-pad **UP↔DOWN** and **LEFT↔RIGHT** swapped; **L3↔R3** swapped in **`SwitchProHost::process_report()`**.

### Switch 2 Pro (PID 0x2069) and Switch 2–family USB — bulk bring-up

Applies to Nintendo VID **0x057E** PIDs **0x2066**, **0x2067**, **0x2069**, **0x2073** (`is_switch2_usb_family()`).

1. **Bulk sequence** — Read configuration descriptor, open **interface 1** bulk IN/OUT, send **17 packets** from **`Switch2UsbInitPackets.h`** (~**12 ms** spacing between rounds), optional bulk IN read per round when an IN endpoint exists.
2. **No post-bulk Switch 1 init** — On successful bulk completion, **`init_state_ = DONE`** immediately and **`tuh_hid_receive_report()`** starts — **Switch 1 `0x80` HID init is skipped** for Switch 2–family devices.
3. **Switch2ProHost** — Parses **64-byte** HID report ID **0x09** (1-byte counter + **10-byte** payload) with **Switch 2–specific** button bits (see v1.0.0.9a / [Wired_Controllers.md](Wired_Controllers.md)).
4. **Async safety** — Bulk callbacks use **`switch2_make_cb_token(daddr, instance)`** and **`HostManager::get_switch_pro_host()`** to look up the live driver; safe after hot-swap or unplug.
5. **Teardown** — **`disconnect_cb()`** / destructor call **`switch2_cancel_bringup()`**: abort in-flight bulk transfers, clear pending retry state.

### DualShock 3 — wired USB host

**Problem:** **DualShock 3** over **USB cable** failed to connect or dropped shortly after plug-in on **PIO USB host** boards. Init used the **USB gadget** default OUT template (leading **`0x01`**) and **async-only** control transfers without servicing **`pio_usb_host_frame()`**, so enable/LED commands never completed reliably.

**Reference:** [USB Host Shield PS3](https://github.com/felis/USB_Host_Shield_2.0) host report buffer / feature **0xF4** enable sequence.

#### Changes

1. **Host OUT report layout** — **`init_host_out_report()`** builds the **USB Host Shield `PS3_REPORT_BUFFER`** layout (player LED bitmap **`0x02 << player`**, per-LED timing fields) instead of copying gadget **`DEFAULT_OUT_REPORT`**.
2. **Synchronous wired init** — On plug-in: **SET feature 0xF4** `{0x42, 0x0c, 0x00, 0x00}` then **synchronous OUT** (rumble/LED report) via **`send_control_xfer_wait()`**, polling **`pio_usb_host_frame()` + `tuh_task()`** up to **150 ms** per step.
3. **Immediate input** — Wired path sets **`reports_enabled`** and **`InitStage::DONE`** after enable + LED OUT; **`tuh_hid_receive_report()`** starts without waiting on the old multi-stage **GET feature 0xF2** chain (that chain remains for **Bluetooth auto-pair** on `CONFIG_EN_BLUETOOTH` builds only).
4. **PIO keepalive** — **`KEEPALIVE_MS = 1000`** on USB host sends periodic neutral OUT so the DS3 stays connected when idle; **`HostManager`** includes PS3 in periodic **`send_feedback()`**.
5. **Async OUT for rumble** — Runtime rumble/keepalive uses non-blocking **`send_control_output_async()`** so feedback does not stall IN.

**File:** `src/USBHost/HostDriver/PS3/PS3.cpp`, `PS3.h`.

**User testing (Waveshare RP2350-USB-A):** **DualShock 3 wired** connects and holds input after flash.

### Xbox 360 wireless receiver — wired USB host

**Problem:** **Microsoft Xbox 360 wireless PC receiver** (`045e:0719`) plugged into the adapter’s **PIO USB host** port often failed to **sync** a wireless pad or delivered **no input** when **`MAX_GAMEPADS=1`**. Only one of four receiver interfaces was stored; **controller-present** was mis-detected (**headset `0x40`** treated as paired); connect path **slept 1 s** and blocked PIO servicing; **`Xbox360WHost`** assumed a fixed struct layout for variable-length wireless reports.

#### Changes

1. **Four XInput instances on single-player builds** — **`OGXM_TUH_XINPUT_INSTANCES = 4`** when `MAX_GAMEPADS < 4`; **`MAX_INTERFACES = MAX_GAMEPADS + OGXM_TUH_XINPUT_INSTANCES`**; **`host_storage_index()`** maps XInput instances to **`MAX_GAMEPADS + instance`**.
2. **One driver, four ports** — **`HostManager`** treats sibling **`XBOX360W`** interfaces as sharing one gamepad slot when `MAX_GAMEPADS=1`; **`process_report` / `connect_cb` / `disconnect_cb`** fall back to the mounted **`Xbox360WHost`** when the per-instance slot has no driver pointer.
3. **`tuh_xinput` pairing** — **`prime_port_for_pairing()`** on each idle port: **RUMBLE_ENABLE + player LED** (quadrant from interface number **0/2/4/6**); called at **set_config** and periodically via **`service_wireless_ports()`** from **`HostManager::send_feedback()`**.
4. **Connect detection** — Controller present when **IN byte1 & 0x80** (not merely non-zero / headset **0x40**); on connect: **RUMBLE_ENABLE**, **`wait_for_tx_complete()`** (200 ms cap), **LED** — **no 1 s sleep**.
5. **Input reports** — Accept **report size 0x13 or 0x14**; **`Xbox360WHost::process_report()`** decodes **button word, triggers, sticks** from raw byte offsets; tracks **`active_instance_`** for rumble on the port that last connected.
6. **Rumble** — **`set_rumble()`** skips **RUMBLE_ENABLE** when already connected; returns false if port not connected (avoids spurious OUT on idle RF slots).

**Files:** `Board/Config.h`, `HostManager.h`, `HostDriver/XInput/tuh_xinput/tuh_xinput.cpp`, `HostDriver/XInput/Xbox360W.cpp`.

**User testing (Waveshare RP2350-USB-A):** **360 wireless receiver + pad** sync and input confirmed over PIO USB host.

### Hot-swap (Switch 1 ↔ Switch 2 Pro)

Unplug during bulk or HID init runs **`HostManager::disconnect_cb()`** → driver **`disconnect_cb()`** → bulk cancel. Plugging the other model starts a fresh init path (Switch 1 **0x80** vs Switch 2 bulk) without requiring an adapter power cycle.

### Files (all wired-host fixes)

| File | Role |
|------|------|
| `src/USBHost/HostManager.h` | PIO **`pio_usb_host_frame()`** on all host boards; PS3/Switch periodic feedback; 360W driver routing; **`disconnect_cb`** on unmount; **`get_switch_pro_host()`** |
| `src/Board/Config.h` | **`OGXM_TUH_XINPUT_INSTANCES`** (4 when `MAX_GAMEPADS < 4`) |
| `src/USBHost/HostDriver/SwitchPro/SwitchPro.cpp` | Switch 1 wired init, Switch 2 bulk bring-up, Switch 1 input mapping, PIO USB service loop |
| `src/USBHost/HostDriver/SwitchPro/SwitchPro.h` | Init state machine, bulk bring-up members, `disconnect_cb` |
| `src/USBHost/HostDriver/SwitchPro/Switch2ProHost.cpp` | Switch 2 Pro wired button decode |
| `src/USBHost/HostDriver/SwitchPro/Switch2UsbInitPackets.h` | 17 bulk OUT packets (capture tool / HHL) |
| `src/Descriptors/SwitchPro.h` | Wired USB constants (`REPORT_ID_USB_OUT`, `USB_SUB_*`, `USB_PROBE`) |
| `src/USBHost/HostDriver/PS3/PS3.cpp` | DualShock 3 wired init, sync control xfer, 1 Hz keepalive |
| `src/USBHost/HostDriver/XInput/tuh_xinput/tuh_xinput.cpp` | 360 wireless receiver port priming, connect detect, **`service_wireless_ports()`** |
| `src/USBHost/HostDriver/XInput/Xbox360W.cpp` | Wireless input decode from byte offsets, active instance rumble |

**Build:** `./scripts/build.sh` → board **RP2350_USB_A**; typical **`-DMAX_GAMEPADS=1`**. Optional **`-DOGXM_SWITCH2_HID_RAW_LOG=ON`** for UART hex when Switch 2 Pro digital button bytes change.

**User testing (Waveshare RP2350-USB-A):** **Switch 2 Pro wired**, **DualShock 3 wired**, and **Xbox 360 wireless receiver + pad** confirmed on PIO USB host. **Switch 1 Pro wired** — init and mapping updated; retest after flash if a prior S2 regression was seen.

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

5. **Wake console from standby (remote wakeup)**
   - When the Xbox 360 has been turned off via **Guide → Turn off console** (not the front power button), the console may keep USB power and put the bus in suspend. The adapter can then wake the console by signaling USB remote wakeup when you:
     - **Press Guide (Home)** on the controller, or
     - **Hold Start for 3 seconds** (avoids holding Guide on Xbox One/PS5 pads, which can turn the controller off).
   - Remote wakeup is advertised in the configuration descriptor; the stack defaults it enabled when supported so wake works even if the host did not send SET_FEATURE before standby.
   - **Disclaimer:** Wake only works when the console was previously powered on with the adapter connected and then turned off via the controller (soft shutdown). It **cannot** power on the console from a cold start—e.g. if the console was just plugged in, lost power completely, or was turned off with the front power button. In those cases use the console’s power button to turn it on first.

Descriptors and XSM3 flow are aligned with [joypad-os](https://github.com/joypad-ai/joypad-os); a local comparison doc may be kept out of the repo for reference.

---

## PS3 mode — input delays, stuck inputs, Home button, and analog stick emulation

**Source:** Fixes from [OGX-Mini-Plus](https://github.com/guimaraf/OGX-Mini-Plus) (v1.1.1) — *“PS3 Driver Fixes - Fixed input delays and stuck inputs.”* Plus subsequent improvements for Home (PS) button and DS3-accurate analog sticks.

**Files:** `src/USBDevice/DeviceDriver/PS3/PS3.cpp`, `src/Descriptors/PS3.h`

### Changes

1. **L2/R2 axis values**
   - `report_in_.l2_axis` and `report_in_.r2_axis` are now set from `gp_in.trigger_l` and `gp_in.trigger_r`.
   - Previously left at zero, which could cause stuck or incorrect trigger behaviour on PS3. Filling these is required for many PS3 games.

2. **DualShock 3–accurate analog sticks**
   - Sticks now match the real DS3/Sixaxis HID spec:
     - **Range:** 0–255 (full 8-bit; was 0–254).
     - **Center:** **0x80 (128)** at rest and in deadzone (was 0x7F). Matches Linux gamepad spec and HID logical max 255.
     - **Deadzone:** ~1.5% (512 on ±32768) so small movements register without drift; in deadzone the report sends 0x80.
     - **Scaling:** Linear map from signed 16-bit input to 0–255 with correct rounding so center (0) → 128.
   - Reduces stick drift and matches console expectations for DS3-compatible games.

3. **D-pad in analog mode**
   - When `gamepad.analog_enabled()` is true, D-pad axes (`up_axis`, `down_axis`, `left_axis`, `right_axis`) are now derived from the **digital** D-pad bits instead of `gp_in.analog[ANALOG_OFF_*]`.
   - Prevents noisy analog D-pad values from causing stuck or wrong D-pad input on PS3.

4. **Face button axes (digital / non-analog branch)**
   - Corrected mapping so that:
     - `circle_axis` = BUTTON_B (was BUTTON_X)
     - `cross_axis` = BUTTON_A (was BUTTON_B)
     - `square_axis` = BUTTON_X (was BUTTON_A)
   - Circle / Cross / Square now match the intended face buttons.

5. **Home (PS) button**
   - Report is built only when `tud_hid_ready()` (right before send), using `gamepad.get_pad_in()` so the console gets the latest state (helps with DS4/DS5 over Bluetooth).
   - **PS button latch:** When `BUTTON_SYS` is set, the driver latches the PS bit for 8 consecutive report frames so short taps are not missed by timing. `buttons[2]` bit 0 (PS) and bit 1 (Touchpad) are set from `BUTTON_SYS` and `BUTTON_MISC`.
   - If Home does not work over Bluetooth, try a wired controller (some consoles only react to the first controller's Home).

6. **Wake console from standby (remote wakeup)**
   - The configuration descriptor now advertises **remote wakeup** (`bmAttributes` 0xA0) so the PS3 can suspend the USB bus in standby and the adapter can signal wake. When the console is in standby (turned off via **PS button → Turn off system** or similar, not full power loss), you can wake it by **pressing PS (Home)** or **holding Start for 3 seconds**—**only if the console keeps USB power when off**.
   - **Many PS3s cut power to the USB ports** when shut down, so the adapter and controller disconnect and wake is not possible. If your controller stays powered (e.g. charging LED) when the PS3 is off, that model may keep USB in standby and wake may work. Same disclaimer as 360: no wake from cold start; use the console power button first if needed.
   - **Recovery Mode:** Even when wake from standby is not possible (e.g. console cuts USB when off), the adapter is **confirmed working in PS3 Recovery Mode** — you can use it to navigate and select options in Recovery.

7. **Host rumble → Bluetooth pad (PS3 mode on PC)** *(v1.0.0.9a)*  
   When the adapter is in **PS3 output mode** and connected to a **Windows** host, the OS still sends **HID output** (rumble) to the device. That output is forwarded to the **Bluetooth** gamepad as `PadOut` rumble. Some Windows stacks leave **noise** in the report (small large-motor values, or non‑`0`/`1` small-motor bytes). Forwarding that blindly caused **constant vibration** on the wireless controller at idle.  
   **Fix:** Ignore large-motor values **below a small threshold** (deadzone), and treat the **small motor as on only when the host byte is `1`** (not “any non-zero”).  
   **File:** `PS3.cpp` — block that runs when `new_report_out_` is set after `set_report_cb` parses the DS3 output report.

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

### XInput (360): report always fresh; send when ready (minimal latency)

Same goal as Switch Pro and PS3: the only added latency when using a wireless controller is the Bluetooth radio.

- **Every** `process()` call: read `get_pad_in()` and build `in_report_` (buttons, triggers, sticks). Then, if suspended, wake; call `tud_xinput::send_report(&in_report_)`. `send_report()` only actually transmits when the IN endpoint is free (`send_report_ready()`); otherwise we keep the latest `in_report_` so that (1) the host’s `get_report_cb` returns current state if it polls, and (2) the next time the endpoint is free we send that report. No `new_pad_in()` gate.
- **File:** `src/USBDevice/DeviceDriver/XInput/XInput.cpp` — `process()` always builds `in_report_` every loop; `tud_xinput::send_report()` sends only when `send_report_ready()` (see `tud_xinput.cpp`).

### Switch Pro and PS3: report always fresh; send when ready (minimal latency)

**Goal:** For Switch Pro and PS3 output modes, the only added latency should be wireless Bluetooth (radio) when using a BT controller. The adapter does not batch, throttle, or delay reports.

- **Switch Pro:** Every `process()` call reads `get_pad_in()`, builds `switch_report_`, and builds the standard or subcommand report into `report_` **before** any USB decisions. So (1) the host’s `get_report` (poll) always gets the latest `report_`, and (2) when `tud_hid_n_ready(0)` we push that same report. Init reply (0x81) is sent first when pending, then the standard report. No “only build when ready” — report is always current.
- **PS3:** Every `process()` call reads `get_pad_in()` and builds `report_in_` (full DS3 report). Then, when `tud_hid_ready()`, we send it. So (1) the host’s `get_report_cb` always returns the latest `report_in_`, and (2) we push that report whenever the IN endpoint is free. No “only build when ready” — report is always current.

Both modes: no `new_pad_in()` gate; main loop runs with `MAIN_LOOP_DELAY_US=0` by default; `tud_task()` runs before `process()` so the endpoint is ready when we try to send.

### Xbox OG (Duke) gamepad: send only on new input (match Team-Resurgent)

Report build/send timing matches [Team-Resurgent/OGX-Mini](https://github.com/Team-Resurgent/OGX-Mini): the HID report is built and sent **only** when `gamepad.new_pad_in()` is true. Sending every poll caused random disconnects on some OG Xbox setups; the “send when new input” rule avoids that. Guide (SYS) combos are kept: **Guide only** = IGR (LT+RT+Start+Back), **Guide+Start** = shutdown (LT+RT+Back+White). Rumble handling is unchanged.

- **File:** `src/USBDevice/DeviceDriver/XboxOG/XboxOG_GP.cpp` — `process()` builds `in_report_` and calls `tud_xid::send_report()` only when `new_pad_in()` and `send_report_ready(0)`.

**DualSense (PS5) input when outputting to OG Xbox:** The PS5 USB host no longer skips reports that are byte-identical to the previous one. Previously, “unchanged” reports did not call `set_pad_in()`, so with a polled output (OG Xbox) some transitions or sustained input could be dropped when the host loop was slower than the DualSense report rate. Every DualSense report is now pushed into the gamepad queue so the OG Xbox device always has the latest state. **File:** `src/USBHost/HostDriver/PS5/PS5.cpp` — removed the unchanged-report early return; every report is parsed and passed to `gamepad.set_pad_in()`.

### Main loop order: tud_task() before process()

The main loop now calls **`tud_task()` before** `device_driver->process()`. That way the USB stack updates completion status of the previous IN transfer first; then `process()` sees the endpoint as ready and can send the next report immediately with the latest gamepad state. Reduces latency by up to one main-loop iteration (avoids sending only every other loop when the host polls frequently).

**Files:** `src/OGXMini/Board/PicoW.cpp`, `Standard.cpp`, `Four_Channel_I2C.cpp` — order is `process_tasks()` → `tud_task()` → `process()` for each gamepad.

---

## PS2 (GPIO) / Open PS2 Loader stability

**Issue:** With a “primed” first response byte (0xFF) before the mode byte, Open PS2 Loader could hang at startup (black screen) when the adapter was connected.

**Fix:** The PS2 controller (device) response was reverted so the **first response byte is the mode byte** (no leading 0xFF). Protocol and escape-mode response lengths otherwise follow PicoGamepadConverter and DS4toPS2. The main loop drains all pending PS2 transactions each tick so rapid pad init (e.g. OPL at boot) does not desync. Core1 runs Bluetooth when used; Core0 runs the main loop and `psx_device_poll()` so the console sees input correctly.

**File:** `src/USBDevice/DeviceDriver/PS1PS2/controller_simulator.c` — first response byte is the mode byte; no `prime_first_byte()` / leading 0xFF.

---

## PS2 (GPIO) and OG Xbox — Home/Guide IGR and shutdown

**Goal:** Use the Home (PS2) or Guide (OG Xbox) button for in-game reset (IGR) and console shutdown with the same interaction pattern on both platforms: **Home only** = restart (IGR), **Home+Start** = shutdown.

### PS2 (GPIO) mode

**Files:** `src/USBDevice/DeviceDriver/PS1PS2/PS1PS2.cpp`, `PS1PS2.h`

- **Home only** — Sends the OPL in-game reset combo: **L1+L2+R1+R2+Start+Select** (triggers full). The console restarts the game / returns to OPL.
- **Home+Start** — Sends shutdown combo: **L1+L2+R1+R2+L3+R3** (triggers full). The console shuts down.

### OG Xbox mode

**Files:** `src/USBDevice/DeviceDriver/XboxOG/XboxOG_GP.cpp`, `XboxOG_GP.h`

- **Guide only** — Sends IGR (restart) combo: **LT+RT+Start+Back** (triggers full). The console performs a soft reset / returns to dashboard (or IGR handler).
- **Guide+Start** — Sends shutdown combo: **LT+RT+Back+White** (triggers full). The console shuts down.

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

## Additional board support (RP2350_ZERO, RP2040_XIAO, RP2354)

**Source:** [Sakura-Research-Lab/OGX-Mini-2026-Testing](https://github.com/Sakura-Research-Lab/OGX-Mini-2026-Testing).

Three additional boards use the same Standard (PIO-USB host) code path as PI_PICO, RP2040_ZERO, ADAFRUIT_FEATHER, and RP2350_USB_A:

| Board | CMake option | Notes |
|-------|--------------|--------|
| Waveshare RP2350-Zero | `OGXM_BOARD=RP2350_ZERO` | PIO USB D+ = GP10, RGB = GP16; RP2350. |
| Seeed Studio XIAO RP2040 | `OGXM_BOARD=RP2040_XIAO` | PIO USB D+ = GP0, RGB = GP12, LED = GP17. |
| RP2354 | `OGXM_BOARD=RP2354` | PIO USB D+ = GP0, LED = GP25; RP2350. |

**Files:** `src/Board/Config.h` (board IDs and pin defines), `src/OGXMini/OGXMini.cpp` (init/run/host_mounted tables), `src/OGXMini/Board/Standard.cpp` (extended `#if`), `CMakeLists.txt` (board branches).

---

## 8BitDo XInput (Xbox 360) host fix

**Source:** [Sakura-Research-Lab/OGX-Mini-2026-Testing](https://github.com/Sakura-Research-Lab/OGX-Mini-2026-Testing).

Some 8BitDo wired XInput controllers (VID 0x2DC8, PID 0x3016 or 0x3106) can disconnect or behave oddly if the host leaves the controller LED on. The Xbox 360 host driver now detects these devices by VID/PID and schedules a repeating delayed task (every 1 s) that sends “LED off” to the controller. Other controllers are unchanged (LED stays on as before).

**File:** `src/USBHost/HostDriver/XInput/Xbox360.cpp` — `initialize()` calls `tuh_vid_pid_get()`, and if 8BitDo, queues `TaskQueue::Core1::queue_delayed_task(..., 1000, true, set_led(..., false))`.

---

## PS5 (DualSense) over Bluetooth — reducing perceived delay vs Xbox One

**Why PS5 can feel slower than Xbox One over BT:** DualSense sends larger reports (78 bytes, with gyro/accel), and Bluepad32’s DS5 parser does more work per report (calibration, etc.). Xbox One reports are smaller and the parser is lighter. Bluetooth poll/report rate and radio latency dominate; the adapter’s job is to not add extra delay.

**Change in this firmware:** The PS5 adaptive-trigger toggle (touchpad/mute button) now runs **after** `set_pad_in(gp_in)`. Previously it ran at the start of the callback; when you pressed the touchpad it could send two output reports (left/right trigger effect) before updating gamepad state, which could delay the next main-loop read. Now the gamepad state is always written first, then the trigger effect is sent, so input is not held up by the trigger command.

**If you need minimum latency with a DualSense:** Use the controller **wired** on the PIO USB host port when possible; wired PS5 uses the same low-latency path as other USB host controllers and avoids BT report size and rate limits.

---

## Pico W / Pico 2 W — PIO USB wired controller unplug detection

**Problem:** On **Pico W / Pico 2 W**, the **USB gamepad** plugs into a **PIO USB** host. While PIO owns **D+ / D−**, reading line state with **`gpio_get()`** (as in **`pio_usb_bus_get_line_state()`** / **`hcd_port_connect_status()`**) often **does not** show a clean **SE0** after you pull the cable — lines can **float** or sit in a state that still looks like full-speed idle. The firmware could keep thinking the port was **connected**, so **TinyUSB** stayed up, **HostManager** still had a slot, and **Bluetooth** stayed blocked (wired takeover) until a power-cycle or “shorting” the port.

**Approach:** In **`pico_w_pio_usb_bt_mux_tick()`** (`src/OGXMini/Board/PicoW.cpp`), treat **unplug** when **`HostManager::any_mounted()`** is still true **and** **any** of these **hints** fires (all share one **debounce**, ~**60 ms** wall time):

1. **`!hcd_port_connect_status(BOARD_TUH_RHPORT)`** — line-based disconnect when the HCD stack *does* see disconnect.
2. **No configured TinyUSB device** — loop device addresses **`1 … CFG_TUH_DEVICE_MAX + CFG_TUH_HUB`** (matches TinyUSB’s internal **`TOTAL_DEVICES`**) and require **`tuh_mounted(d)`** for at least one address. If the stack has dropped configuration, tear down even if the line hint lied.
3. **No USB host input for several seconds** — **`HostManager::usb_host_input_idle_ms()`** (default threshold **3000 ms** in `PicoW.cpp`) — after unplug, **IN transfers** and **`process_report()`** usually stop even if (1) and (2) lag. **`record_usb_host_input_activity()`** is called from **`process_report()`** and after successful **`setup_driver()`** (`src/USBHost/HostManager.h`). **`HostManager::initialize()`** seeds **`last_usb_host_input_ms_`** from **`board_api::ms_since_boot()`** so idle time is not huge before the first device.

After debounced confirmation, **`pico_w_usb_host_full_stop()`** runs **`tuh_deinit`**, stops the SOF timer, clears unplug debounce state, and **`board_api_usbh::enable_host_line_irq_monitoring()`** so normal **GPIO unplug/plug** IRQs work again; **Bluetooth** release paths run as before.

**Tuning:** If unplug feels slow, lower **`USB_UNPLUG_NO_INPUT_MS`** in `PicoW.cpp`. If a controller that **rarely sends reports** when idle ever mis-triggers unplug, raise that constant slightly.

---

## DualShock 3 — automatic USB programming for Bluetooth pairing

**Background:** The **DualShock 3** does not implement standard Bluetooth pairing. The host’s **BD_ADDR** must be written to the pad over **USB** using a **HID feature report** (report ID **`0xF5`**, 8 bytes: id + padding + 6-byte MAC). This is what the [Bluepad32 sixaxispairer](https://bluepad32.readthedocs.io/en/latest/pair_ds3/) utility does from a PC.

**Behavior in this firmware:** On builds with **`CONFIG_EN_BLUETOOTH`** (e.g. **Pico W / Pico 2 W**, ESP32 hybrids with Bluepad32), after the existing **PS3 USB init** (`GET_REPORT` **0xF2** sequence) completes, **`PS3Host`** sends **`SET_REPORT`** **feature 0xF5** with **`uni_bt_get_local_bd_addr_safe()`**, then continues with the usual rumble **OUT** report. If the local address is still **all zeros** (Bluetooth stack not finished starting), the code sets a **deferred** flag and **`try_deferred_ds3_bt_pair()`** runs at the start of **`process_report()`** until the address is valid, then sends **0xF5** once.

**User steps:** Plug the **DS3 into the adapter’s USB host port** (wait for it to work wired if you like), then **unplug** and press **PS** to use it wirelessly — same as the manual pairing flow documented for Bluepad32.

**Files:** `src/USBHost/HostDriver/PS3/PS3.cpp`, `PS3.h`.

---

## Pico W / Pico 2 W — DualShock 4 and Classic Bluetooth (BR/EDR) stability

**Context:** On **CYW43439** (Pico W / Pico 2 W), **BLE** and **Classic Bluetooth (BR/EDR)** share one radio. **DualShock 4** uses **Classic ACL** only. **DualSense**, **Xbox Series (BLE)**, and **Switch Pro** (typical pairing) use **LE** — so DS4 was uniquely sensitive to how the stack used the radio at the same time as other activity.

### 1. BLE advertising vs Classic ACL (main DS4 drop fix)

**OGX-Mini** starts **BLE advertising** via **BLEServer** so the **phone / web app** can discover the adapter. If that advertising stays on while a **Classic** gamepad (DS4, DualShock 3 BT, Xbox 360 wireless) holds an **ACL** link, the connection often **dies within seconds** (e.g. blue LED then disconnect).

- **`gap_advertisements_enable(0)`** at the **start of DS4 HID setup** (before lightbar / calibration traffic), and again whenever **any** ready gamepad uses **`GAP_CONNECTION_ACL`**.
- **`gap_advertisements_enable(1)`** when the **last** Classic pad disconnects (BLE-only controllers, e.g. DualSense still connected, are unaffected by this rule).

**Files:** `Firmware/external/bluepad32/.../parser/uni_hid_parser_ds4.c` (OGXM Pico path), `Firmware/RP2040/src/Bluepad32/Bluepad32.cpp`.

**User impact:** While a **Classic Bluetooth** controller is connected, the adapter **does not advertise** for the BLE web app. Disconnect that controller (or use **USB** for setup) to use **Bluetooth** in the web app again.

### 2. BR/EDR inquiry while Classic is connected

**Periodic inquiry** (scanning for new gamepads) **plus** an active **Classic ACL** also contends on the same radio and contributed to **short-lived DS4** links.

- **`uni_bt_bredr_scan_stop()`** at the beginning of DS4 setup and when any **ACL** pad becomes **ready**.
- **`uni_bt_bredr_scan_start()`** when the **last** Classic pad disconnects (if scanning is still enabled globally).

**File:** `Bluepad32.cpp` (and DS4 setup in `uni_hid_parser_ds4.c`).

### 3. DS4 virtual “touchpad mouse” disabled on Pico

Bluepad32 can register a **second virtual HID device** (mouse) on the same DS4 link. On Pico W that path was linked to **unstable links**; the OGX build (**`OGXM_BLUEPAD32_PICO_W`**) **does not create** that virtual device. **DualSense** still uses its normal setup (virtual child rejected in `device_ready` where applicable).

**File:** `uni_hid_parser_ds4.c` (CMake defines `OGXM_BLUEPAD32_PICO_W=1` for the Bluepad32 library in `Firmware/RP2040/CMakeLists.txt`).

### 4. PS4 rumble / FF grace period

For **`CONTROLLER_TYPE_PS4Controller`**, **rumble output** is **delayed ~6 seconds** after connect so the host cannot push force-feedback during the fragile init window.

**File:** `Bluepad32.cpp` — `send_feedback_cb` / `device_ready_cb`.

### 5. Related Pico W Bluetooth improvements (see also Summary table)

- **Core0 `sleep_ms(1)`** in the main loop so Core1 (BT stack) gets CPU time ([Team-Resurgent/OGX-Mini](https://github.com/Team-Resurgent/OGX-Mini) pattern).
- **Lock-free Bluetooth input path** (`set_pad_in_from_bluetooth`) so HID callbacks never block on Core0’s mutex. **Joy-Con pairs:** **2-slot** staging ring so back-to-back L/R reports are not overwritten before Core0 drains (see [§ Joy-Con pair merge — latency](#joy-con-pair-merge--latency-when-both-halves-are-active)).
- **Reconnect:** last device disconnect calls **`uni_bt_enable_new_connections_unsafe(true)`** so pairing can resume without power-cycling.
- **8 s input-stall disconnect** (non-virtual, non-BLE-Xbox) to clear zombie links; **BLE Xbox** uses keepalive instead of stall disconnect.

### 6. Connection rumble when the wireless controller is ready *(v1.0.0.9a)*

When Bluepad32 reports **`device_ready`** for a non-virtual gamepad, the firmware plays about **one second** of **dual-motor rumble** (via `play_dual_rumble` when the controller supports it) so you can **feel** that the Bluetooth link is up. **DualShock 4** uses a **~1.2 s delay** before starting that rumble so it does not overlap the fragile post-connect window (aligned with the existing PS4 rumble grace logic). Other controller types use a shorter default delay.

**File:** `src/Bluepad32/Bluepad32.cpp` — `ogxm_play_connection_rumble()`, called from `device_ready_cb`.

---

## Switch Pro — analog stick sensitivity

A configurable sensitivity gain is applied to the analog sticks in Switch Pro emulation. Raw stick values (outside the deadzone) are scaled by **STICK_GAIN_NUM / STICK_GAIN_DEN** (default 120/100 = 1.2×) before mapping to the 12-bit Switch report. The same physical deflection produces slightly larger output for a more responsive feel.

**File:** `src/USBDevice/DeviceDriver/Switch/Switch.cpp` — `gamepad_to_switch_report()`; tune via `STICK_GAIN_NUM` and `STICK_GAIN_DEN`.

---

## Build scripts (new users)

To build firmware without memorizing CMake options, use the interactive build scripts from the **project root**:

| Platform | Command |
|----------|---------|
| **Linux / macOS** | `./scripts/build.sh` |
| **Windows (PowerShell)** | `.\scripts\build.ps1` |

The script checks for required tools (git, python3, cmake, ninja, arm-none-eabi-gcc) and prints install hints if something is missing. It then prompts for: (1) board (Pi Pico, Pico W, Pico 2 W, RP2040-Zero, XIAO, Feather, 4CH I2C, ESP32 hybrid, etc.); (2) default (all modes via combos) or fixed output mode (e.g. Wii, GameCube, N64); (3) Release or Debug. Build output (`.uf2`, `.elf`) is written to **`scripts/build/`**; on failure you can save a log to `scripts/build_log.txt`. See the main [README](../../../README.md) Build section for the full description.

---

## Future planned

Work that is **not implemented** in current firmware but has been researched. These items are **not** a matter of adding a VID/PID to a table — they need new host drivers and/or radio stacks.

### Xbox Wireless Adapter for Windows (`045e:02e6`, `045e:02fe`)

**Devices:**

| VID:PID | Model | USB product string | Status |
|---------|-------|-------------------|--------|
| **`045e:02e6`** | 1713 | Xbox Wireless Adapter for Windows (older) | **Not supported** |
| **`045e:02fe`** | 1790 | **XBOX ACC** (newer) | **Not supported** |

These are **USB dongles** that receive **Xbox Wireless** (proprietary 2.4 GHz) from Xbox One and Xbox Series controllers. They are **not** gamepads and do **not** enumerate as XInput devices.

**Do not confuse with the Xbox 360 PC receiver** — that **is supported** today:

| VID:PID | Device | Why it works |
|---------|--------|--------------|
| **`045e:0719`** (typical) | Xbox 360 wireless **PC receiver** | Presents a standard XInput-style USB interface (`bInterfaceSubClass=0x5D`, `bInterfaceProtocol=0x81`). Handled by `tuh_xinput` / `Xbox360WHost`. |

The One/Series dongles use a **completely different USB shape** (vendor bulk, not XInput interrupt).

#### What the dongle looks like on USB

From hardware dumps (e.g. [ControllersInfo adapter descriptor](https://github.com/DJm00n/ControllersInfo/blob/master/xboxone/DescriptorDump_Adapter%20(Xbox%20Wireless%20Adapter%20for%20Windows).txt)):

- **Device class:** vendor-specific (`0xFF/0xFF/0xFF`), product name **"XBOX ACC"**
- **Endpoints:** **bulk** IN/OUT (512-byte packets on USB 2.0), not the interrupt endpoints used by **wired** Xbox One pads
- **Inside the stick:** a **Mediatek MT76** Wi‑Fi chipset that must run **dongle firmware** before it can talk to controllers

OGX-Mini’s existing **wired Xbox One GIP** path (`tuh_xinput`, subclass `0x47` / protocol `0xD0`, `XboxOneHost`) parses GIP **after** the controller is already connected over USB. The wireless adapter never exposes that interface — the host must drive the **dongle radio** first.

#### Why this is not a simple port

1. **Firmware upload** — Linux [xone](https://github.com/medusalix/xone) / [xow](https://github.com/medusalix/xow) load **`xow_dongle.bin`** / variant blobs (e.g. **`xone_dongle_02fe.bin`**) into the MT76 chip over USB before the device is useful. That blob must be shipped in flash and the load sequence reimplemented on RP2040.

2. **Wireless stack, not HID** — The driver brings up **MT76** (channels, pairing scan, client join/leave, optional encryption), wraps **GIP** payloads in **802.11-style data frames**, and sends them on bulk OUT queues. Inbound bulk IN carries WLAN frames that must be parsed to extract GIP input. This is the bulk of [xone `transport/dongle.c`](https://github.com/medusalix/xone/blob/master/transport/dongle.c) + [`transport/mt76.c`](https://github.com/medusalix/xone/blob/master/transport/mt76.c) — thousands of lines, not a report-descriptor tweak.

3. **Resource cost** — xone uses many bulk URBs, WLAN buffers up to tens of KB per packet, and ongoing radio work. RP2040 flash/RAM and Core1 USB host timing are tight compared to a PC kernel driver.

4. **Pairing model** — Controllers must be **paired to the dongle** (Sync on the pad while the dongle is in pairing mode). Pads previously used over **USB** or **Bluetooth** will not auto-attach until re-paired to the dongle — same as on Windows with xone.

5. **Reuse is partial only** — Once a wireless client is connected and GIP input arrives, decoding could **reuse** existing GIP constants and mapping from `Descriptors/XboxOne.h` / `XboxOneHost.cpp`. Everything **before** that (USB dongle + radio + framing) is new work — comparable in scope to adding Switch 2 bulk bring-up, but **larger** because of firmware + Wi‑Fi.

**Rough implementation phases (if pursued):**

1. New TinyUSB **vendor bulk** class driver; claim `045e:02e6` / `045e:02fe`; embed firmware; port MT76 init from xone.  
2. Bulk I/O loops, pairing, client add/remove (mirror `Xbox360W` connect callbacks in `HostManager`).  
3. Bridge decoded GIP `0x20` / `0x07` reports into `XboxOneHost` (or a shared GIP parser).  
4. Rumble/LED over wireless GIP OUT path.

**Practical alternatives today:**

- **Xbox 360 wireless receiver** + 360 pads — already supported (`Xbox360WHost`).  
- **Xbox One / Series controller** — **Bluetooth** on Pico W / Pico 2 W (Bluepad32), or **wired USB** (`XboxOneHost`).  

**References:** [medusalix/xone](https://github.com/medusalix/xone), [medusalix/xow](https://github.com/medusalix/xow), [SDL discussion of 02fe vs XInput PID](https://github.com/libsdl-org/SDL/pull/8683), [ControllersInfo dongle descriptor dump](https://github.com/DJm00n/ControllersInfo/tree/master/xboxone).

**Files that would be touched (when implemented):** new `tuh_xbox_dongle` (or similar) under `USBHost/HostDriver/XInput/`, `HostManager.h`, `tuh_callbacks.cpp`, `tusb_config.h`; possible shared GIP layer with `XboxOne.cpp`.

---

## Summary

| Area | Improvement |
|------|-------------|
| **XInput (360)** | XSM3 authentication and descriptors aligned with joypad-os; adapter works on Xbox 360 with BT controllers (PS5, Xbox One). **360 wireless PC receiver** supported (`Xbox360WHost`). **v1.0.0.11a:** PIO USB wired receiver — **4 XInput instances** when `MAX_GAMEPADS=1`, port **priming**, **0x80** connect detect, byte-offset decode. **Xbox One/Series wireless dongle (`045e:02e6` / `02fe`)** — see [Future planned](#future-planned). 8BitDo wired fix: LED keepalive for VID 0x2DC8 / PID 0x3016 or 0x3106. |
| **PS3** | Stuck inputs and delays addressed via L2/R2 axes; DS3-accurate sticks (0–255, center 0x80, ~1.5% deadzone); D-pad and face button mapping; Home (PS) button with 8-frame latch for BT controllers. **v1.0.0.9a:** PC host rumble deadzone + strict small-motor `0`/`1`. **v1.0.0.11a:** **DualShock 3 wired USB host** — USB Host Shield init, sync control xfer + PIO service, **1 Hz keepalive**. |
| **PS2 (GPIO)** | Home only = IGR (L1+L2+R1+R2+Start+Select); Home+Start = shutdown (L1+L2+R1+R2+L3+R3). OPL and protocol stability (first response byte = mode byte). |
| **OG Xbox** | Guide only = IGR. Shutdown = LT+RT+Back+White via **Guide+Start** or **Guide+View (Back)**; Xbox BT often omits Start while Guide is held. Shutdown report strips Start so the chord matches BIOS/softmod expectations. |
| **Switch Pro** | Analog stick sensitivity gain (default 1.2×) for more responsive sticks; configurable in `Switch.cpp`. **v1.0.0.11a:** **Switch 1/2 Pro wired** on PIO USB host — Chromium **0x80** init, bulk bring-up, mapping fixes. See [§ PIO USB host — wired connection fixes](#pio-usb-host--wired-connection-fixes-waveshare-rp2350-usb-a). |
| **Switch 2** | **Pro 2** (wired **0x2069**): **Switch2ProHost** + bulk bring-up (v1.0.0.11a). **Pro 2 + Joy-Con 2 L/R** (BLE): **uni_hid_parser_switch2** — GATT pairing, 63-byte input, rumble keepalive, Home → SYS latch, **L+R pair merge**, **dual-half latency fixes**. See [§ Nintendo Switch 2 — Bluetooth](#nintendo-switch-2--bluetooth-pico-w--pico-2-w) and [§ Joy-Con pair latency](#joy-con-pair-merge--latency-when-both-halves-are-active). |
| **Boards** | RP2350_ZERO, RP2040_XIAO, RP2354 supported (Standard/PIO-USB host path). **RP2350-USB-A (Waveshare):** v1.0.0.11a validation for **Switch Pro, DualShock 3, 360 wireless receiver** wired host. |
| **Latency** | Main loop delay default **0 µs**; `tud_task()` before `process()` so reports send every loop when ready; XInput/Switch/PS3 send latest state when USB ready (no `new_pad_in()` gate). Switch Pro and PS3 always build report every loop so host poll (`get_report`) and IN push both see current state — only remaining delay is BT radio when wireless. |
| **Build** | Interactive scripts `scripts/build.sh` (Linux/macOS) and `scripts/build.ps1` (Windows) for board selection, fixed/default mode, and Release/Debug; output in `scripts/build/`. See [README](../../../README.md) Build section. |
| **Bluetooth (Pico W)** | **DS4 / Classic ACL:** BLE advertising **paused** while Classic pad connected; **BR inquiry stopped** during ACL; **no DS4 virtual mouse**; **6 s PS4 rumble** grace. **Xbox Series (BLE):** no stall disconnect when idle; keepalive 12 s; stale-slot delete on reconnect. **Switch 2 (BLE):** Pro 2 + Joy-Con 2 L/R — custom GATT parser, rumble keepalive, Home latch, **L+R merge**, **dual-half latency fixes**. **Switch 1 Joy-Con (Classic BT):** L+R merge, IMU off when paired, same latency path as SW2. **General:** `sleep_ms(1)` main loop; lock-free BT pad-in (**2-slot** staging for Joy-Con pairs); re-enable scan on last disconnect. **v1.0.0.9a:** ~**1 s connection rumble** at `device_ready` (DS4 delayed start). |
| **PIO USB host (Pico W)** | **Wired unplug:** Debounced combo of **HCD connect**, **`tuh_mounted` over all device addresses**, and **no `process_report` / setup activity** (~**3 s**) so disconnect registers when D+/D− line state is wrong under PIO; **`tuh_deinit`** + restore GPIO line IRQs + BT release. **v1.0.0.11a (all PIO host boards):** **`pio_usb_host_frame()`** after feedback OUT; wired **Switch Pro / PS3 / 360 receiver** connection fixes — see [§ PIO USB host — wired connection fixes](#pio-usb-host--wired-connection-fixes-waveshare-rp2350-usb-a). |
| **DS3 + Bluetooth** | **USB auto-pair:** After PS3 wired init, **feature `0xF5`** programs the **DS3** with the adapter’s **BD_ADDR** (`CONFIG_EN_BLUETOOTH`); deferred if BT address not ready. See [§ DualShock 3 — automatic USB programming for Bluetooth pairing](#dualshock-3--automatic-usb-programming-for-bluetooth-pairing). |
