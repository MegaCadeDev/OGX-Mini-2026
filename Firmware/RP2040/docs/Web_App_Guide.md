# Web app guide: checking a controller and sending a diagnostics report

The [web app](https://megacadedev.github.io/OGX-Mini-2026-WebApp/) changes mappings and deadzones, and it is also the easiest way to see what the adapter receives from a controller. Its **Diagnostics** panel shows how well each controller's link is doing, and **Generate log report** saves a file to attach when [opening a support issue](Support_Issue_Requirements.md).

**Related**

| Topic | Document |
|-------|----------|
| What a support issue must include | [Support_Issue_Requirements.md](Support_Issue_Requirements.md) |
| Input → output mapping reference | [Controller_Mappings.md](Controller_Mappings.md) |

---

## Connecting

1. Plug the adapter into a PC.
2. Put it in **Web App mode**: hold **Start + Left Bumper + Right Bumper** for about 3 seconds. The adapter restarts in that mode.
3. Open the web app in a Chromium-based browser (Chrome, Edge, Brave): Web Serial is not available in Firefox or Safari.
4. Click **Connect via USB** and pick the OGX-Mini.

The Diagnostics panel works over USB only. Over Bluetooth, the mapping settings are available but not the report.

---

## Checking a controller

With the controller connected to the adapter:

- **Sticks and triggers:** the Axis Settings panel draws the live position of each stick and trigger. Tick **Preview** to see it after the deadzone and curve of the selected profile. A stick that rests off center, or does not reach the edge, shows up here.
- **Link health:** open **Diagnostics** and click **Refresh** while moving the sticks (some controllers only report when something changes). Each controller gets a row:

| Column | Meaning |
|--------|---------|
| Link | Classic or LE Bluetooth, the LE connection interval, power saving (sniff) if the controller is in it; USB speed for wired controllers |
| Reports/s | Input reports per second the adapter receives |
| Late | Share of reports that arrived more than twice the usual interval after the previous one (last 5-10 s) |
| Lost | Reports the controller numbered but the adapter never got (DualShock 4 and DualSense, which count their reports) |
| Largest gap | Longest time without a report in the last 5-10 s |
| Signal | Bluetooth signal, when the adapter reports it |
| Connected | Time since the controller connected |

As a guide, a DualShock 4 over Bluetooth next to the adapter shows about 250 reports/s, under 1 % late and lost, and a largest gap under 30 ms. Gaps of 100 ms or more are felt as stutter in a game. High late / lost values usually come from distance, obstacles or 2.4 GHz interference (Wi-Fi, other wireless devices), a low battery, or the adapter searching for new controllers (shown above the table).

---

## When to generate a report

Generate a report and attach it to the issue whenever the problem is about how the adapter behaves, for example:

- input lag, stutter, or buttons that sometimes do not register;
- a controller that disconnects, does not pair, or does not reconnect;
- the adapter freezing, restarting by itself, or ending up in the wrong mode.

It is not a replacement for the adapter-side report capture required for [mapping problems and new controllers](Adding_Supported_Controllers.md#step-2--capture-reports-on-the-adapter-required-for-driver-mapping).

---

## How to generate a report

1. Use the adapter in the mode you play with until the problem happens.
2. **Without unplugging it**, switch to Web App mode (**Start + Left Bumper + Right Bumper**, about 3 s).
3. Connect the web app over USB, open **Diagnostics** and click **Generate log report**. A file named `ogx-mini-report-<date>.json` is saved.
4. Attach that file to the issue, and say which mode and controller the problem happened with.

Measurements of the mode you played with are kept across the switch into Web App mode, so the report covers them (`previous_session`). If the adapter was unplugged in between, generate the report anyway: the summary is also stored on the adapter when you change modes and when the last controller turns off (`stored_session`). If the adapter crashed, it restarts by itself and the report includes what it recorded (`last_crash`).

### What the report contains

The adapter's board, firmware version and build, output mode, and uptime; for each controller its name, USB / Bluetooth IDs, the first half of its Bluetooth address (the manufacturer part only), its link and the timing figures above; a list of recent events (connections, disconnections with the reason, mode changes, searches for new controllers); the summary of the previous session; and the browser's version. It contains no personal data.
