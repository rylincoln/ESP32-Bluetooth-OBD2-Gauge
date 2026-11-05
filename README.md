# ESP32-Bluetooth-OBD2-Gauge

A fork of [VaAndCob/ESP32-Bluetooth-OBD2-Gauge](https://github.com/VaAndCob/ESP32-Bluetooth-OBD2-Gauge), the ESP32 OBD2 gauge created by Ratthanin W. (Va&Cob). The gauge's design, features, hardware and enclosure are their work. The original project was last updated on September 27th, 2024. Its repository was archived on October 24th, 2025 and is no longer maintained.

> **Work in progress, not yet tested on a device.** This fork's firmware restructuring is still under way. Its changes pass their automated tests and compile for both Ford variants, but they haven't run on a gauge or in a car yet. For the original author's release, see [Original firmware](#original-firmware).

This is an ESP32 OBD2 Gauge for vehicles that are compatible with OBD2
(Special made for FORD vehicle)
 - Prototype https://youtu.be/PkQaUJbzTNM
 - Current Model Short Demo https://youtu.be/vvBIeim7XTE
   
![](/picture/page.jpg)
![](/picture/config.jpg)
![](enclosure/enclosure.jpg)

## What this fork changes

The gauge is meant to look and work as it did, apart from the fixes below. Saved settings keep the same names and types, so a gauge should keep its settings when updated.

### Fixes

- **Bluetooth scan:** the adapter only connected if it was the last device found.
- **Reconnecting:** the gauge could report a failure when the adapter connected on its last check.
- **DTC screen:**
  - A reply with one or two codes could run past the end of a buffer, and `NO DATA` could hang the screen.
  - Codes whose first byte is 43, such as C0301, were lost or misread.
  - Padding bytes could appear as false codes.
  - The error screen could never appear.
- **Memory:** a warning in meter cell 9, a scan finding more than 8 devices, or a saved layout number one too high could go past the end of an array.
- **Meters:** a reply meant for another PID was drawn as the current one, and an empty reply reused old bytes. The meter now keeps its last value.
- **Auto-off:** layouts 3 and 5 shut down in about half the time of the others. Every layout now counts PCM volts once per round.

### Code structure (work in progress)

The restructuring isn't finished, so expect more of the firmware to move. So far, reply decoding, the PID and layout tables, the ELM327 connection, button gestures, saved settings and the shutdown decisions now live in separate modules with no Arduino dependencies, so their tests run on a computer with no hardware attached:

```sh
make -C test
```

[GLOSSARY.md](GLOSSARY.md) defines the terms the code uses.

## Software:
- Arduino IDE 2.3.2 + ESP32 arduino core 2.0.17
- * IMPORTANT * this project won't work with ESP32 arduiino core 3.x.x
- modified TFT_eSPI library 2.5.43
- Copy `sketch/library modified/TFT_eSPI/User_Setup.h` over the installed TFT_eSPI library's file
- JPEGDecoder 2.0.0 and XPT2046_Touchscreen 1.4.0 libraries (versions known to compile)
- Board settings are in `sketch/arduinoSetup.png`
- The default build is for Ford T6+. For Ford T5, uncomment `#define FORD_T5` at the top of `sketch/VaAndCobOBD2Gauge/gauge_tables.cpp`

## Hardware
- ESP32 TFT2.8 320x240 with "RESISTIVE" touch board.
buy here  https://s.click.aliexpress.com/e/_Ew95gMl 
- ELM327 Bluetooth adaptor (recommended v1.5 not v2.1)
  Highly recommend ELM327 Bluetooth  adaptor https://s.click.aliexpress.com/e/_oo3THvG
- A small speaker for mobile phone 1 Watt 8 ohm 9x28 mmm.
- A push button 6x6x5 2 legs
- 3d print enclosure
- 12v to 5v dc micro usb power regulator module
- Gauge magnetic stand

## Features:
- Show vehicle data 7 pids
* MAP - manifold air pressure (PSI)
* PCM - pcm voltage (volt)
* ENG LOAD - engine load (%)
* ENG SPD - engine speed (RPM)
* Coolant - coolant temperature (c)
* Oil Temp - engine oil temperature (c)
* TFT - Ford transmission fluid temperature (c)

- 8 layout page selectable display
- DTC read and clear function (Engine warning light only)
- Warning when parameter reaches setting value
- Adjustable warning value for each PIDs
- Configurable CPU overheat protection
- Configurable gauge Auto turn on/off
- Auto screen brightness
- Change off screen to user screen with micro SDcard
- Firmware updatable (micro SDcard and WiFi)
- VIN Read on the About page.


## Original firmware

[Flash Firmware Online](https://vaandcob.github.io/webpage/src/index.html) installs the original author's release. That release and the binaries in `sketch/VaAndCobOBD2Gauge/firmware/` don't include this fork's changes, so to run this fork, build it from source using the settings above.

## Credits

- **Original project:** created by Ratthanin W. ([Va&Cob](https://www.facebook.com/vaandcob)), in the [original repository](https://github.com/VaAndCob/ESP32-Bluetooth-OBD2-Gauge). The demo videos, product links and online flasher in this README are theirs too.
- **NES audio player:** [Cartridge](https://github.com/connornishijima/Cartridge) by Connor Nishijima, included as `nes_audio.h` and `nes_audio.cpp`, released under GPLv3.

## License

[LICENSE.md](LICENSE.md) is the original project's license, unchanged: MIT terms plus a restriction against commercial use, which also applies to derivative works such as this fork. Cartridge's own GPLv3 notice is at the top of `nes_audio.h`.

-------------------------------------------------------
Support the original author:

[![Buy Me a Coffee](https://img.buymeacoffee.com/button-api/?text=Buy%20me%20a%20coffee&emoji=☕&slug=vaandcob&button_colour=FFDD00&font_colour=000000&font_family=Cookie&outline_colour=000000&coffee_colour=ffffff)](https://www.buymeacoffee.com/vaandcob)


