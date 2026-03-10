# OGX-Mini 2026
![OGX-Mini Boards](images/OGX-Mini-github.jpg "OGX-Mini Boards")

Firmware for the RP2040, capable of emulating gamepads for several game consoles. The firmware comes in many flavors, supported on the [Adafruit Feather USB Host board](https://www.adafruit.com/product/5723), Pi Pico, Pi Pico 2, Pi Pico W, Pi Pico 2 W, Waveshare RP2040-Zero, Waveshare RP2350-USB-A, Waveshare RP2350-Zero, Seeed Studio XIAO RP2040, RP2354, Pico/ESP32 hybrid, and a 4-Channel RP2040-Zero setup.

[**Visit the web app here**](https://megacadedev.github.io/OGX-Mini-2026-WebApp/) to change your mappings and deadzone settings. To pair the OGX-Mini with the web app via USB, plug your controller in, then connect it to your PC, hold **Start + Left Bumper + Right Bumper** to enter web app mode. Click "Connect via USB" in the web app and select the OGX-Mini. You can also pair via Bluetooth, no extra steps are needed in that case. 

[**Join the discord here!**](https://discord.gg/guaBh9JZQ)

## Supported platforms
- Original Xbox
- Playstation 3
- Nintendo Switch 1 & 2 (docked) — **Switch Pro Controller** emulation over USB
- XInput (Xbox 360, UsbdSecPatch no longer required)
- Playstation Classic
- DInput
- Wii U (GameCube Adapter)
- **Wii (Wiimote)** — Pico W / Pico 2 W only; build with `-DOGXM_FIXED_DRIVER=WII`. See [Wii Mode Guide](Firmware/RP2040/docs/Wii_Mode_Guide.md).
- **PlayStation 1 & 2** — GPIO output to console controller port (DualShock-style).
- **Dreamcast** — GPIO output over Maple Bus to console controller port.
- **GameCube / Wii (GameCube ports)** — GPIO output over single-wire JoyBus. **Build-only (no combo):** use `-DOGXM_FIXED_DRIVER=GAMECUBE`; see below.
- **Nintendo 64** — GPIO output over single-wire to controller port. **Build-only (no combo):** use `-DOGXM_FIXED_DRIVER=N64`; see below.

**RP2040 output modes:** **USB device:** XInput (Xbox 360 + XSM3), DInput, PS3, **Switch Pro** (Nintendo Switch Pro Controller emulation), Wii U, **Wii (Wiimote, build-option only)**, Xbox OG (Gamepad / Steel Battalion / XRemote), PS Classic, Web App. **GPIO (no USB device):** PS1/PS2, Dreamcast, GameCube, N64 — use any wired USB or (on Pico W / Pico 2 W) Bluetooth controller as input. **Wii, GameCube, and N64 are not in the combo list** — use a dedicated build for those modes (see below).

## Changing platforms
By default the OGX-Mini will emulate an OG Xbox controller, you must hold a button combo for 3 seconds to change which platform you want to play on. Your chosen mode will persist after powering off the device. 

Start = Plus (Switch) = Options (Dualsense/DS4)

- XInput (Xbox 360)
    - Start + Dpad Up 
- Original Xbox
    - Start + Dpad Right
- Original Xbox Steel Battalion
    - Start + Dpad Right + Right Bumper
- Original Xbox DVD Remote
    - Start + Dpad Right + Left Bumper
- Switch (Pro Controller emulation)
    - Start + Dpad Down
- PlayStation 3
    - Start + Dpad Left
- PlayStation Classic
    - Start + A (Cross for PlayStation and B for Switch gamepads)
- PlayStation 1 / PlayStation 2 (GPIO output)
    - Start + B
- Dreamcast (GPIO output)
    - Start + Y
- Wii U (GameCube Adapter)
      - Start + Left Bumper + D-Pad Down
- Web Application Mode
    - Start + Left Bumper + Right Bumper

**Wii, GameCube, and N64 are not selectable by combo.** Use a dedicated build for those modes:

- **Wii (Wiimote):** `-DOGXM_FIXED_DRIVER=WII`. See [Wii Mode Guide](Firmware/RP2040/docs/Wii_Mode_Guide.md).
- **GameCube (GPIO):** `-DOGXM_FIXED_DRIVER=GAMECUBE`. Example (Pico 2 W Debug):  
  `cmake -DOGXM_BOARD=PI_PICO2W -DCMAKE_BUILD_TYPE=Debug -DOGXM_FIXED_DRIVER=GAMECUBE ..` then `ninja`. Flash `OGX-Mini-*-GAMECUBE.uf2`. Use `PI_PICOW` for Pico W.
- **N64 (GPIO):** `-DOGXM_FIXED_DRIVER=N64`. Example (Pico 2 W Debug):  
  `cmake -DOGXM_BOARD=PI_PICO2W -DCMAKE_BUILD_TYPE=Debug -DOGXM_FIXED_DRIVER=N64 ..` then `ninja`. Flash `OGX-Mini-*-N64.uf2`.

Input for GPIO modes is Bluetooth or USB; no combo switching in these builds.

After a new mode is stored, the RP2040 will reset itself so you don't need to unplug it.

## Disconnecting Controllers
For most controllers pressing and holding Start+Select (+/-, etc) for the controller will disconnect it and restart pairing mode.
For the OUYA controller there is no Start+Select, the disconnection combo has been set to L3+R3.

## Supported devices
### Wired controllers (USB input)
The following work when the adapter is outputting to any supported platform (Xbox, Switch, XInput, PS3, DInput, **PS1/PS2**, **Dreamcast**, **GameCube**, **N64**, etc.):
- Original Xbox Duke and S
- Xbox 360, One, Series, and Elite
- Dualshock 3 (PS3)
- Dualshock 4 (PS4)
- Dualsense (PS5)
- Nintendo Switch Pro
- Nintendo Switch wired
- Nintendo 64 Generic USB
- Playstation Classic
- Generic DInput
- Generic HID (mappings may need to be editted in the web app)

Note: There are some third party controllers that can change their VID/PID, these might not work correctly.

### Wireless adapters
- Xbox 360 PC adapter (Microsoft or clones)
- 8Bitdo v1 and v2 Bluetooth adapters (set to XInput mode)
- Most wireless adapters that present themselves as Switch/XInput/PlayStation controllers should work

### Wireless Bluetooth controllers (Pico W & ESP32)
**Note:** Bluetooth functionality is in early testing, some may have quirks. Bluetooth controllers work as input when outputting to **PS1/PS2**, **Dreamcast**, **GameCube**, or **N64** (same as wired USB in those modes).
- Xbox Series, One, and Elite 2
- Dualshock 3
- Dualshock 4
- Dualsense
- Switch Pro
- Steam
- Stadia
- Wii U Pro
- Wii Remote
   - Supported Extensions:
      - Gamepad
      - Nunchuck
      - GameCube Controller
- 8BitDo Ultimate Wireless (Switch layout)

Please visit [**this page**](https://bluepad32.readthedocs.io/en/latest/supported_gamepads/) for a more comprehensive list of supported controllers and Bluetooth pairing instructions.

# Features new to this fork:
Note: These features have been added to the Pico W/ Pico 2 W firmware support, I do not have the other boards to test and implement the same fixes at this time.

### Version 1.0.0.6a
- **PS1/PS2 (GPIO) output mode** — The adapter can output to a PS1 or PS2 console as a controller over GPIO (no USB device in this mode). Select via button combo **Start + B** or set a fixed driver build. **Wiring (standard Pico):** connect the board to the console’s controller port using DATA=GP19, COMMAND=GP20, ATTENTION/SEL=GP21, CLOCK=GP22, ACKNOWLEDGE=GP26. Plug your USB gamepad into the adapter’s USB port; the console sees a DualShock-style controller. _Protocol and PIO code ported from [PicoGamepadConverter](https://github.com/Loc15/PicoGamepadConverter)._
- **GameCube (GPIO) output mode** — The adapter can output to a GameCube or Wii (GameCube ports) as a controller over a single DATA line. Select via button combo **Start + X** or set a fixed driver build. **Wiring (standard Pico):** DATA=GP19, GND=GND; connect to the console’s controller port (single wire + ground). Plug your USB gamepad into the adapter’s USB port. _JoyBus protocol and PIO code ported from [PicoGamepadConverter](https://github.com/Loc15/PicoGamepadConverter)._
- **N64 (GPIO) output mode** — The adapter can output to a Nintendo 64 console as a controller over a single DATA line. Select via button combo **Start + Right Bumper** or set a fixed driver build. **Wiring (standard Pico):** DATA=GP19, GND=GND; connect to the N64 controller port (same pin as GameCube). Use any **wired (USB)** or **Bluetooth** (Pico W / Pico 2 W) gamepad as input. _Protocol and PIO send code ported from [pdaxrom/usb2n64-adapter](https://github.com/pdaxrom/usb2n64-adapter). Memory pak and rumble pak are not emulated._
- **Dreamcast (Maple Bus)** — **Input:** Read a real Dreamcast controller over GPIO (Pin A=GP10, B=GP11). **Output:** Use a USB or **Bluetooth** gamepad to control a Dreamcast: select **Start + Y** for Dreamcast mode, connect your BT or USB pad, wire the adapter’s Maple Bus (GP10/GP11) to the console’s controller port. Core1 runs the Maple device (responds to device info and GET_CONDITION); Core0 feeds gamepad state from USB/BT. When input source is **Dreamcast GPIO**, the bus is used as host only (read a DC pad). See [Dreamcast Port guide](Firmware/RP2040/docs/Dreamcast_Port.md). Maple Bus from [DreamPicoPort](https://github.com/OrangeFox86/DreamPicoPort).
- **Pin-outs and default mappings** — See [GPIO Output Pin-Out and Mappings](Firmware/RP2040/docs/GPIO_Output_Pinout_and_Mappings.md) for pin numbers (PS1/PS2, Dreamcast, GameCube, N64) and default button/stick mappings for each mode.
- **Bluetooth input for PS1/PS2, GameCube, Dreamcast, and N64** — On **Pico W** (or Pico 2 W), you can use a **Bluetooth controller** as input while outputting to PS1, PS2, GameCube, Dreamcast, or N64. Select the mode (**Start + B**, **Start + X**, **Start + Y**, or **Start + Right Bumper**) as usual; pair your BT gamepad; the console sees the adapter as a controller. Wiring is the same for each mode (PS1/PS2: DATA/CMD/ATT/CLK/ACK; GameCube/N64: DATA=GP19 + GND; Dreamcast: GP10/GP11).
- **GPIO input from real PS1/PS2, GameCube, N64, or Dreamcast controllers** — You can use a **real wired PS1/PS2, GameCube, N64, or Dreamcast controller** as the adapter’s input in two ways: (1) **Output to other consoles (Switch, Xbox, DInput, etc.):** set output mode (e.g. **Start + D-pad Down** for Switch), then hold **Start + Select** for ~2.5 s until the input source cycles to **PSX GPIO**, **GameCube GPIO**, **Dreamcast GPIO**, or **N64 GPIO**; plug the real controller into the GPIO pins and connect the adapter’s USB to the target console. (2) **Output to PS1/PS2, GameCube, or Dreamcast:** in that output mode, hold **Start + Select** to set input to the matching GPIO and plug in the real controller (same pins). **Wiring:** PS1/PS2: DATA=GP19, CMD=GP20, ATT=GP21, CLK=GP22; GameCube/N64: DATA=GP19 + GND; Dreamcast: GP10, GP11. _PSX host, JoyBus host, N64 host ported from [PicoGamepadConverter](https://github.com/Loc15/PicoGamepadConverter)._

### Version 1.0.0.5a
- **Switch Pro emulation** — Switch output mode now emulates a **Nintendo Switch Pro Controller** over USB (report format, subcommands, init handshake). Face-button and report layout follow the standard Pro Controller protocol (bit layout aligned with hardware/reference). _Switch Pro protocol and button report layout reference: [retro-pico-switch](https://github.com/DavidPagels/retro-pico-switch) (N64/GameCube → Pico → Switch); Bluepad32 Switch parser used for validation._
- **Waveshare RP2350-USB-A board support** — Build with `-DOGXM_BOARD=RP2350_USB_A` for the Waveshare RP2350-USB-A (wired USB host + RGB LED). _Board support from upstream [OGX-Mini](https://github.com/wiredopposite/OGX-Mini/commit/68bdec451077a33b0b1247ce01ed30e470163dea)._
- **Additional board support** — **RP2350_ZERO** (Waveshare RP2350-Zero), **RP2040_XIAO** (Seeed Studio XIAO RP2040), and **RP2354**. All use the same Standard (PIO-USB host) firmware path; build with `-DOGXM_BOARD=RP2350_ZERO`, `RP2040_XIAO`, or `RP2354`. _Board support from [Sakura-Research-Lab/OGX-Mini-2026-Testing](https://github.com/Sakura-Research-Lab/OGX-Mini-2026-Testing)._
- **8BitDo XInput (Xbox 360) fix** — Wired 8BitDo controllers (VID 0x2DC8, PID 0x3016 / 0x3106) are detected and sent a repeating LED-off keepalive so they stay connected and stable. _From [Sakura-Research-Lab/OGX-Mini-2026-Testing](https://github.com/Sakura-Research-Lab/OGX-Mini-2026-Testing)._
- **Switch analog stick sensitivity** — Slight sensitivity increase (1.2× gain) for analog sticks in Switch Pro emulation mode; configurable via `STICK_GAIN_NUM` / `STICK_GAIN_DEN` in `Switch.cpp`.
- **Original Xbox (OG) Guide button behavior** — In OG Xbox mode, the Guide (SYS) button is mapped to Start; **tap** = Start, **hold 1 s** = soft IGR (LT+RT+Start+Back), **hold 3 s** = system shutdown (LT+RT+D-pad Up+Back). _Implementation based on [Sakura-Research-Lab/OGX-Mini-2026-Testing](https://github.com/Sakura-Research-Lab/OGX-Mini-2026-Testing)._

### Version 1.0.0.4a
- **Wii (Wiimote) output mode** — On Pico W and Pico 2 W, build with `-DOGXM_FIXED_DRIVER=WII` for a Wii-only firmware. The adapter appears as a Wiimote over Bluetooth; use a USB gamepad on the external PIO USB port. Supports No Extension, Nunchuk, and Classic Controller report modes (cycle with Home + D-pad Down). See [Wii Mode Guide](Firmware/RP2040/docs/Wii_Mode_Guide.md). _Approach and mappings based on [PicoGamepadConverter](https://github.com/wiredopposite/PicoGamepadConverter)._
- Xbox 360 no longer requires the USB patch to authenticate, uses the same authentication as Joypad-OS to do official handshake with Retail consoles. Homebrew/ Jailbreaking is no longer required to use the XINPUT mode on 360 but it will no longer output to PC.
- **Rumble fix for Xbox 360 controllers** — Rumble on 360 pads is now handled correctly when using the adapter (including wireless RUMBLE_ENABLE sequence). _Xbox 360 vibration/rumble handling attributed to [faithvoid](https://github.com/faithvoid) on GitHub._
- **Pico 2 / Pico 2 W build fixes** — Firmware builds and runs correctly on RP2350 (Pico 2 and Pico 2 W).
- Improved latency on Xbox 360 and PS3 controllers.
- **PS3 mode:** Home (PS) button works with DS4/DS5 over Bluetooth (8-frame latch so short taps register). Analog sticks now match DualShock 3 spec: full 0–255 range, center 0x80 (128), ~1.5% deadzone for accurate emulation. See [IMPROVEMENTS.md](Firmware/RP2040/docs/IMPROVEMENTS.md#ps3-mode--input-delays-stuck-inputs-home-button-and-analog-stick-emulation).
- Added build flags so you can make it only output in a specific mode and disable mode switching.
- Using a PS5 controller allows you to tap the touchpad to enable or disable the adaptive triggers.

### Version 1.0.0.3a patched
- Ability to emulate the GameCube controller adapter for Wii U. (Also tested on Switch 1/ 2)
- Wii U & Switch Pro controllers are supported fully with working LT and RT
- Wii Remotes are supported along with the following controllers connected: Nunchuck/ GameCube/ Wii Gamepad.
- Disconnection Combo has been added: Start+Select for most controllers, L3+R3 for OUYA controllers.

## Features new to v1.0.0
- Bluetooth functionality for the Pico W, Pico 2 W, and Pico+ESP32.
- Web application (connectable via USB or Bluetooth) for configuring deadzones and buttons mappings, supports up to 8 saved profiles.
- Pi Pico 2 and Pico 2 W (RP2350) support.
- Reduced latency by about 3-4 ms, graphs showing comparisons are coming.
- 4 channel functionality, connect 4 Picos and use one Xbox 360 wireless adapter to control all 4.
- Delayed USB mount until a controller is plugged in, useful for internal installation (non-Bluetooth boards only). 
- Generic HID controller support.
- Dualshock 3 emulation (minus gyros), rumble now works.
- Steel Battalion controller emulation with a wireless Xbox 360 chatpad.
- Xbox DVD dongle emulation. You must provide or dump your own dongle firmware, see the Tools directory.
- Analog button support on OG Xbox and PS3.
- RGB LED support for RP2040-Zero and Adafruit Feather boards.

## Planned additions from the original creator
- More accurate report parser for unknown HID controllers
- Hardware design for internal OG Xbox install
- Hardware design for 4 channel RP2040-Zero adapter
- Wired Xbox 360 chatpad support
- Wired Xbox One chatpad support
- Switch (as input) rumble support
- OG Xbox communicator support (in some form)
- Generic bluetooth dongle support
- Button macros
- Rumble settings (intensity, enabled/disable, etc.)

## Planned additions for this fork
- **Web app bindings for OG Xbox mode** — Allow users to rebind the Guide tap (Start) and the IGR/shutdown hold combos (e.g. which button triggers 1 s soft IGR or 3 s shutdown) via the web app.
- Output to the following consoles:
      - ~~PS2~~ (done: PS1/PS2 GPIO mode, see changelog)
      - ~~GameCube~~ (done: GameCube GPIO mode, see changelog)
      - DreamCast
      - NES
      - SNES
      - Genesis
      - Master System

## Hardware
For Pi Pico, RP2040-Zero, 4 channel, and ESP32 configurations, please see the hardware folder for diagrams.

I've designed a PCB for the RP2040-Zero so you can make a small form-factor adapter yourself. The gerber files, schematic, and BOM are in Hardware folder.

<img src="images/OGX-Mini-rpzero-int.jpg" alt="OGX-Mini Boards" width="400">

If you would like a prebuilt unit, you can purchase one, with cable and Xbox adapter included, from the original creators store: [**Etsy store**](https://www.etsy.com/listing/1426992904/ogx-mini-controller-adapter-for-original).

## Adding supported controllers
If your third party controller isn't working, but the original version is listed above, send me the device's VID and PID and I'll add it so it's recognized properly.

## Build
### RP2040
Build with **CMake** from the `Firmware/RP2040` directory. You can compile for different boards with the CMake argument ```OGXM_BOARD```:

- ```PI_PICO``` 
- ```PI_PICO2``` 
- ```PI_PICOW``` 
- ```PI_PICO2W``` 
- ```RP2040_ZERO``` (Waveshare RP2040-Zero)
- ```RP2350_USB_A``` (Waveshare RP2350-USB-A)
- ```RP2350_ZERO``` (Waveshare RP2350-Zero)
- ```RP2040_XIAO``` (Seeed Studio XIAO RP2040)
- ```RP2354```
- ```ADAFRUIT_FEATHER``` 
- ```ESP32_BLUEPAD32_I2C```
- ```ESP32_BLUERETRO_I2C``` 
- ```EXTERNAL_4CH_I2C```

You can also set ```MAX_GAMEPADS``` (if &gt; 1, only DInput/PS3 and Switch Pro are supported). **Optional:** ```OGXM_FIXED_DRIVER``` to lock output mode (e.g. ```XINPUT```, ```PS3```); ```OGXM_FIXED_DRIVER_ALLOW_COMBOS=ON``` to keep combos when fixed. ```MAIN_LOOP_DELAY_US``` (default ```0```) sets main-loop delay for lower CPU use (e.g. ```250```).

You'll need git, python3, CMake, Ninja and the GCC ARM toolchain installed. CMake scripts will patch some files in Bluepad32 and BTStack and also make sure all git submodules (plus their submodules and dependencies) are downloaded. Here's an example on Windows:
```
git clone --recursive https://github.com/MegaCadeDev/OGX-Mini-2026.git
cd OGX-Mini-2026/Firmware/RP2040
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DOGXM_BOARD=PI_PICOW -DMAX_GAMEPADS=1
cmake --build build
```
Outputs (`.elf`, `.uf2`, etc.) are in the build directory; flash the `.uf2` to the board. Or use the GCC ARM toolchain and CMake Tools extension in VSCode.

**Latency:** The adapter sends the latest gamepad state whenever the USB endpoint is free (XInput and similar), and the main loop has no added delay by default, so the host gets updates at its poll rate with minimal latency. See [Firmware/RP2040/docs/IMPROVEMENTS.md](Firmware/RP2040/docs/IMPROVEMENTS.md#latency-reduction) for details.

### Firmware documentation
| Document | Description |
|---------|-------------|
| [Wii_Mode_Guide.md](Firmware/RP2040/docs/Wii_Mode_Guide.md) | Wii mode (build-option only): No Extension / Nunchuk / Classic, USB host, sync and auto-connect, button mapping. |
| [PICO2W_WII_USB_SETUP.md](Firmware/RP2040/docs/PICO2W_WII_USB_SETUP.md) | Pico 2 W / Pico W: USB host wiring (PIO USB), pins, build, troubleshooting for Wii mode. |
| [IMPROVEMENTS.md](Firmware/RP2040/docs/IMPROVEMENTS.md) | Firmware improvements: PS3 fixes, latency, XInput/360. |

### ESP32
Please see the Hardware directory for a diagram showing how to hookup the ESP32 to your RP2040.

You will need ESP-IDF v5.1, esptool, python3, and git installed. If you use VSCode, you can install the ESP-IDF extension and configure the project for ESP-IDF v5.1, it'll download everything for you and then you just click the build button at the bottom of the window.

When you build with ESP-IDF, Cmake will run a python script that copies the necessary BTStack files into the components directory, this is needed since BTStack isn't configured as an ESP-IDF component when you download it with git. 


# Credit to the original creator [https://wiredopposite.github.io/](https://github.com/wiredopposite/OGX-Mini/tree/master) for the original base of the project!

## Other projects that have helped enhance this fork

- **[Sakura-Research-Lab/OGX-Mini-2026-Testing](https://github.com/Sakura-Research-Lab/OGX-Mini-2026-Testing)** — OG Xbox Guide button (tap/hold IGR/shutdown), board support for RP2350_ZERO, RP2040_XIAO, RP2354, and 8BitDo XInput (Xbox 360) LED keepalive fix.
- **[Joypad OS](https://github.com/joypad-ai/joypad-os)** — Reference for Xbox 360 (XSM3) authentication with retail consoles; XInput mode on 360 follows the same approach. Descriptors and XSM3 flow are aligned with joypad-os; single config, full XSM3 security string, init/verify in main loop; USB is initialized before Core1 so the 360 can enumerate while BT loads.
- **[Bluepad32](https://github.com/ricardoquesada/bluepad32)** — Bluetooth controller support (Pico W / Pico 2 W).
- **[faithvoid](https://github.com/faithvoid)** — Xbox 360 controller vibration/rumble fix (host-side rumble handling, including wireless RUMBLE_ENABLE sequence).
- **[PicoGamepadConverter](https://github.com/wiredopposite/PicoGamepadConverter)** — Wii (Wiimote) output mode: approach of USB gamepad on PIO USB with Bluetooth reserved for the Wiimote link, and button/stick mappings for No Extension, Nunchuk, and Classic Controller report modes.
- **[retro-pico-switch](https://github.com/DavidPagels/retro-pico-switch)** — Reference for Nintendo Switch Pro Controller protocol and report layout (N64/GameCube → Raspberry Pi Pico → Switch via USB or Bluetooth). Used to align OGX-Mini’s Switch mode with correct Pro Controller button byte layout and emulation behavior.

## Licenses and third-party code

- **Xbox 360 console authentication:** XInput builds that work on retail Xbox 360 consoles use [libxsm3](https://github.com/InvoxiPlayGames/libxsm3) (Xbox Security Method 3). libxsm3 is licensed under the GNU Lesser General Public License v2.1 or later; see `Firmware/external/libxsm3/LICENSE.txt`. libxsm3 credits oct0xor, emoose (ExCrypt), and sanjay900; see `Firmware/external/libxsm3/README.md`.
- **Other dependencies:** See the root `LICENSE` file and the `Firmware/external` directory for Bluepad32, BTStack, TinyUSB, Pico SDK, and other third-party licenses.

