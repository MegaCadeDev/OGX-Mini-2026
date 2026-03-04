# OGX-Mini 2026
![OGX-Mini Boards](images/OGX-Mini-github.jpg "OGX-Mini Boards")

Firmware for the RP2040, capable of emulating gamepads for several game consoles. The firmware comes in many flavors, supported on the [Adafruit Feather USB Host board](https://www.adafruit.com/product/5723), Pi Pico, Pi Pico 2, Pi Pico W, Pi Pico 2 W, Waveshare RP2040-Zero, Waveshare RP2350-USB-A, Pico/ESP32 hybrid, and a 4-Channel RP2040-Zero setup.

[**Visit the web app here**](https://wiredopposite.github.io/OGX-Mini-WebApp/) to change your mappings and deadzone settings. To pair the OGX-Mini with the web app via USB, plug your controller in, then connect it to your PC, hold **Start + Left Bumper + Right Bumper** to enter web app mode. Click "Connect via USB" in the web app and select the OGX-Mini. You can also pair via Bluetooth, no extra steps are needed in that case. 

## Supported platforms
- Original Xbox
- Playstation 3
- Nintendo Switch 1 & 2 (docked) — **Switch Pro Controller** emulation over USB
- XInput (Xbox 360, UsbdSecPatch no longer required)
- Playstation Classic
- DInput
- Wii U (GameCube Adapter)
- **Wii (Wiimote)** — Pico W / Pico 2 W only; build with `-DOGXM_FIXED_DRIVER=WII`. See [Wii Mode Guide](Firmware/RP2040/docs/Wii_Mode_Guide.md).

**RP2040 output modes (USB device):** XInput (Xbox 360 + XSM3), DInput, PS3, **Switch Pro** (Nintendo Switch Pro Controller emulation), Wii U, **Wii (Wiimote, build-option only)**, Xbox OG (Gamepad / Steel Battalion / XRemote), PS Classic, Web App. On Pico 2 W, Bluetooth runs on Core1 and USB on Core0; you can build with a fixed mode or use button combos to switch at runtime (Start + D-pad held ~3 s). **Wii is not in the combo list** — use a Wii-only build to get Wiimote output.

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
- Wii U (GameCube Adapter)
      - Start + Left Bumper + D-Pad Down
- Web Application Mode
    - Start + Left Bumper + Right Bumper

**Wii (Wiimote)** is not selectable by combo. Build with `-DOGXM_FIXED_DRIVER=WII` for a Wii-only firmware; see [Wii Mode Guide](Firmware/RP2040/docs/Wii_Mode_Guide.md).

After a new mode is stored, the RP2040 will reset itself so you don't need to unplug it.

## Disconnecting Controllers
For most controllers pressing and holding Start+Select (+/-, etc) for the controller will disconnect it and restart pairing mode.
For the OUYA controller there is no Start+Select, the disconnection combo has been set to L3+R3.

## Supported devices
### Wired controllers
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
**Note:** Bluetooth functionality is in early testing, some may have quirks.
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

==== Version 1.0.0.5a ===
- **Switch Pro emulation** — Switch output mode now emulates a **Nintendo Switch Pro Controller** over USB (report format, subcommands, init handshake). Face-button and report layout follow the standard Pro Controller protocol (bit layout aligned with hardware/reference). _Switch Pro protocol and button report layout reference: [retro-pico-switch](https://github.com/DavidPagels/retro-pico-switch) (N64/GameCube → Pico → Switch); Bluepad32 Switch parser used for validation._
- **Wii (Wiimote) output mode** — On Pico W and Pico 2 W, build with `-DOGXM_FIXED_DRIVER=WII` for a Wii-only firmware. The adapter appears as a Wiimote over Bluetooth; use a USB gamepad on the external PIO USB port. Supports No Extension, Nunchuk, and Classic Controller report modes (cycle with Home + D-pad Down). See [Wii Mode Guide](Firmware/RP2040/docs/Wii_Mode_Guide.md). _Approach and mappings based on [PicoGamepadConverter](https://github.com/wiredopposite/PicoGamepadConverter)._
- Xbox 360 no longer requires the USB patch to authenticate, uses the same authentication as Joypad-OS to do official handshake with Retail consoles. Homebrew/ Jailbreaking is no longer required to use the XINPUT mode on 360 but it will no longer output to PC.
- **Rumble fix for Xbox 360 controllers** — Rumble on 360 pads is now handled correctly when using the adapter (including wireless RUMBLE_ENABLE sequence). _Xbox 360 vibration/rumble handling attributed to [faithvoid](https://github.com/faithvoid) on GitHub._
- **Pico 2 / Pico 2 W build fixes** — Firmware builds and runs correctly on RP2350 (Pico 2 and Pico 2 W).
- **Waveshare RP2350-USB-A board support** — Build with `-DOGXM_BOARD=RP2350_USB_A` for the Waveshare RP2350-USB-A (wired USB host + RGB LED). _Board support from upstream [OGX-Mini](https://github.com/wiredopposite/OGX-Mini/commit/68bdec451077a33b0b1247ce01ed30e470163dea)._
- **Original Xbox (OG) Guide button behavior** — In OG Xbox mode, the Guide (SYS) button is mapped to Start; **tap** = Start, **hold 1 s** = soft IGR (LT+RT+Start+Back), **hold 3 s** = system shutdown (LT+RT+D-pad Up+Back). _Implementation based on [Sakura-Research-Lab/OGX-Mini-2026-Testing](https://github.com/Sakura-Research-Lab/OGX-Mini-2026-Testing)._
- Improved latency on Xbox 360 and PS3 controllers.
- **PS3 mode:** Home (PS) button works with DS4/DS5 over Bluetooth (8-frame latch so short taps register). Analog sticks now match DualShock 3 spec: full 0–255 range, center 0x80 (128), ~1.5% deadzone for accurate emulation. See [IMPROVEMENTS.md](Firmware/RP2040/docs/IMPROVEMENTS.md#ps3-mode--input-delays-stuck-inputs-home-button-and-analog-stick-emulation).
- Added build flags so you can make it only output in a specific mode and disable mode switching.
- Using a PS5 controller allows you to tap the touchpad to enable or disable the adaptive triggers.
 
=== Version 1.0.0.3a patched ===
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
      - GameCube
      - PS2
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
- ```RP2040_ZERO``` 
- ```RP2350_USB_A``` (Waveshare RP2350-USB-A)
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

- **[Joypad OS](https://github.com/joypad-ai/joypad-os)** — Reference for Xbox 360 (XSM3) authentication with retail consoles; XInput mode on 360 follows the same approach. Descriptors and XSM3 flow are aligned with joypad-os; single config, full XSM3 security string, init/verify in main loop; USB is initialized before Core1 so the 360 can enumerate while BT loads.
- **[Bluepad32](https://github.com/ricardoquesada/bluepad32)** — Bluetooth controller support (Pico W / Pico 2 W).
- **[faithvoid](https://github.com/faithvoid)** — Xbox 360 controller vibration/rumble fix (host-side rumble handling, including wireless RUMBLE_ENABLE sequence).
- **[PicoGamepadConverter](https://github.com/wiredopposite/PicoGamepadConverter)** — Wii (Wiimote) output mode: approach of USB gamepad on PIO USB with Bluetooth reserved for the Wiimote link, and button/stick mappings for No Extension, Nunchuk, and Classic Controller report modes.
- **[retro-pico-switch](https://github.com/DavidPagels/retro-pico-switch)** — Reference for Nintendo Switch Pro Controller protocol and report layout (N64/GameCube → Raspberry Pi Pico → Switch via USB or Bluetooth). Used to align OGX-Mini’s Switch mode with correct Pro Controller button byte layout and emulation behavior.

## Licenses and third-party code

- **Xbox 360 console authentication:** XInput builds that work on retail Xbox 360 consoles use [libxsm3](https://github.com/InvoxiPlayGames/libxsm3) (Xbox Security Method 3). libxsm3 is licensed under the GNU Lesser General Public License v2.1 or later; see `Firmware/external/libxsm3/LICENSE.txt`. libxsm3 credits oct0xor, emoose (ExCrypt), and sanjay900; see `Firmware/external/libxsm3/README.md`.
- **Other dependencies:** See the root `LICENSE` file and the `Firmware/external` directory for Bluepad32, BTStack, TinyUSB, Pico SDK, and other third-party licenses.

