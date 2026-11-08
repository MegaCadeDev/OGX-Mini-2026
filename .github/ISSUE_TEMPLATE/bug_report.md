---
name: Bug report
about: Create a report to help us improve
title: ''
labels: ''
assignees: ''

---

**Describe the bug**
A clear and concise description of what the bug is.

**What type of controller do you have the issue with?**
Please include the model, even better to include the PID/VID. 
You can get the PID and VID like this:
- plug the controller into your computer
- open Device Manager
- right click the controller (probably listed under Human Interface Devices)
- select Properties
- go to the Details tab
- select Hardware IDs from the dropdown menu
It will look like this: HID\VID_046D&PID_C05A

**What platform are you using the OGX-Mini on?**
 OG Xbox, PS3, Switch, etc. 

**What board are you using?**
Pi Pico, Adafruit Feather, RP2040-Zero...

**Diagnostics log report**
For lag, disconnects, pairing problems or the adapter freezing / restarting: right after the problem, switch the adapter to Web App mode (Start + Left Bumper + Right Bumper, 3 s) without unplugging it, connect the web app over USB, open Diagnostics, click Generate log report and attach the file here. Steps: Firmware/RP2040/docs/Web_App_Guide.md

See Firmware/RP2040/docs/Support_Issue_Requirements.md for everything a support issue needs.
