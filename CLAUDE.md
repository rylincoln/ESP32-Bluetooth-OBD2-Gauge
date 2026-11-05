# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Arduino firmware for an ESP32-2432S028 board (2.8" 320x240 ILI9341, resistive touch) that polls a Bluetooth ELM327 adapter and draws OBD-II gauges, built for Ford vehicles. `sketch/VaAndCobOBD2Gauge/` is the firmware and `sketch/gauge_factory_init/` is a one-shot provisioning sketch. `origin` is the user's fork and `upstream` is the original author's archived repo, so changes go to `origin`.

## Build and verify

`make -C test` runs the hardware-free modules on this computer and needs only a C++ compiler. There's no linter or CI. After a firmware change, run the tests and compile both vehicle variants. Anything the tests don't reach needs the physical gauge: report a change as compile-checked until the user has flashed it.

Toolchain pins:

- **ESP32 Arduino core 2.0.17.** The README rules out core 3.x, which removed the `ledcSetup` and `ledcAttachPin` calls this code uses.
- **TFT_eSPI 2.5.43**, with `sketch/library modified/TFT_eSPI/User_Setup.h` copied over the installed library's file. The display driver and pins are defined only there, and reinstalling the library overwrites it. Without the copy, the build fails with `'TFT_BL' was not declared`.
- **JPEGDecoder 2.0.0** and **XPT2046_Touchscreen 1.4.0** are also required. The README omits them and the author's versions aren't recorded, so these are simply versions that compile.

The author builds with Arduino IDE 2.3.2 using the board settings in `sketch/arduinoSetup.png`. These `arduino-cli` commands reproduce those settings. The install and compile steps were tested with arduino-cli 1.5.1 on Apple Silicon, where builds need Rosetta unless you set up the native tools described below. The flash and monitor commands have not been run, and no build made this way has been on a device:

```sh
URL=https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index --additional-urls "$URL"
arduino-cli core install esp32:esp32@2.0.17 --additional-urls "$URL"
arduino-cli lib install TFT_eSPI@2.5.43 JPEGDecoder@2.0.0 XPT2046_Touchscreen@1.4.0
# then copy User_Setup.h over <sketchbook>/libraries/TFT_eSPI/User_Setup.h

FQBN=esp32:esp32:esp32:PartitionScheme=min_spiffs,FlashMode=dio,FlashFreq=80,FlashSize=4M,PSRAM=disabled,CPUFreq=240

arduino-cli compile --fqbn "$FQBN" sketch/VaAndCobOBD2Gauge         # Ford T6+ (default)
arduino-cli compile --fqbn "$FQBN" sketch/VaAndCobOBD2Gauge \
  --build-property compiler.cpp.extra_flags=-DFORD_T5               # Ford T5
arduino-cli compile --fqbn "$FQBN" sketch/gauge_factory_init
arduino-cli compile --fqbn "$FQBN" -u -p <port> sketch/VaAndCobOBD2Gauge   # build and flash
arduino-cli monitor -p <port> -c baudrate=115200
```

Compile output reports flash use. The firmware fills about 94% of its 1.9 MB app slot, so large additions can overflow it.

Debug toggles are the commented-out `#define`s at the top of the `.ino` and can be passed with `-D` the same way (quote the value when passing several): `SERIAL_DEBUG`, `TERMINAL` (ELM327 replies on screen), `TEST_DTC` (canned DTC data). Lines typed into the serial monitor are forwarded to the adapter as commands, which is handy for probing AT commands and PIDs.

### Building without Rosetta

On Apple Silicon, the core's `esptool` and Arduino's built-in `ctags` are the only x86_64 programs a compile runs. Native builds of the same versions replace them. With `arduino-cli`, `gauge_factory_init` then builds to byte-identical images. The firmware embeds a build timestamp, so for T6 and T5 only the generated `.ino.cpp` was confirmed identical.

```sh
N="$HOME/.arduino-native"
python3 -m venv "$N/venv"
"$N/venv/bin/pip" install esptool==4.5.1

mkdir -p "$N/ctags-src" "$N/ctags-build" "$N/ctags"
curl -fsSL https://github.com/arduino/ctags/archive/refs/tags/5.8-arduino11.tar.gz \
  | tar -xz -C "$N/ctags-src" --strip-components=1
# ctags' __unused__ macro corrupts __unused from Apple's sys/cdefs.h, so rename it
grep -rl --include='*.[ch]' __unused__ "$N/ctags-src" | xargs sed -i '' 's/__unused__/ctags_unused/g'
(cd "$N/ctags-build" && "$N/ctags-src/configure" CFLAGS=-O2 && make -j4)
cp "$N/ctags-build/ctags" "$N/ctags/ctags"
```

Then put these lines in `packages/esp32/hardware/esp32/2.0.17/platform.local.txt` under the Arduino data directory (`~/Library/Arduino15` by default), with `<N>` written as an absolute path:

```
tools.esptool_py.path=<N>/venv/bin
tools.esptool_py.cmd=esptool.py
tools.ctags.path=<N>/ctags
```

## Architecture

### One translation unit, ordered by includes

The `.ino` declares the globals, then includes `image.h` and `touchscreen.h` (line 185), defines a few shared functions, then includes `bluetooth.h`, `meter.h` and `config.h` (line 271). `config.h` pulls in `music.h`, `nes_audio.h`, `firmware_update.h` and `elm327.h`. Headers hold definitions and use whatever sits above them, and Arduino only generates prototypes for `.ino` functions, so each header is included exactly once and include order is load-bearing. `nes_audio.cpp` (vendored [Cartridge](https://github.com/connornishijima/Cartridge) NES audio player) is a separate compile unit, and so is each hardware-free module (such as `obd_reply.cpp`). Those modules use no Arduino headers or firmware globals, so `test/` can build them directly, and the core compiles them as `gnu++11`. `beep()` and `clickSound()` live in `firmware_update.h`.

`image.h` is code for its first 106 lines and generated RGB565 arrays after that. `music.h` is VGM bytes on very long lines. Read both with line limits.

### Polling loop

`loop()` keeps one request in flight. A `>` from the ELM327 sets `prompt`, and the next pass hands `bt_message` to `updateMeter()` and sends the next PID. `pidReadSkip` polls slow-moving values less often. Drawing shares this thread, so slow draws lower the poll rate.

`obd_reply` turns reply text into values. It depends on `elm327Init`: it splits replies on spaces and skips `pidCode` length / 2 bytes, which needs headers and echo off (`ATH0`, `ATE0`) with spaces left on. `setup()` also treats the list positionally: it sends an extra CR after index 1 (`ATBRD23`) and waits 2 s after the last entry (`0100`).

The DTC and VIN screens use a separate blocking path: `getPID()` in `elm327.h`, with decoding in `obd_reply`. Discovery matches the exact Bluetooth name `OBDII`, and the README recommends a v1.5 adapter over v2.1.

### Three index spaces

Meter code uses three different indexes, often in the same function:

- **PID index** (0-6) is a row in `pidConfig` and `warningValue`. `parameterList` and `pidWarnStep` use the same order offset by one, because their entry 0 is the CPU temperature offset.
- **Slot** (0-6) is a position in the current layout's poll order: `pidIndex`, `pidList`, `pidReadSkip`, `old_data`. `pidInCell[layout][slot]` gives the PID index.
- **Cell** is a screen position: 1-9 for arc and numeric meters, 1-4 for vertical bars. Cells are hard-coded per layout in `initScreen()` and `updateMeter()`.

To change a layout, keep the `pidInCell` row and its `case` in both `initScreen()` and `updateMeter()` in agreement. Slots a layout doesn't draw call `engine_onoff()`. Every layout routes slot 6 there, and auto-off only fires when that slot holds PID 4 (PCM volts, `0142`), so keep PID 4 in slot 6.

To add a PID, update `pidConfig`, `warningValue`, `pidInCell`, `parameterList` and `pidWarnStep`, add a `Formula` and its `case` in `toReading()`, and raise the count hard-coded in those array sizes and in `initScreen()`.

`FORD_T5` swaps two `pidConfig` rows: oil temperature becomes IAT, and transmission temperature moves from `221E1C` to `221674` with a different formula.

### Blocking screens

`configMenu()` takes over from `loop()` with its own `while (true)`, and each sub-screen blocks again on `while (digitalRead(SELECTOR_PIN) == HIGH)`. These wait loops call `autoDim()` and `checkCPUTemp()` so overheat protection stays live, and new screens should do the same. The menu sets `prompt = true` so polling resumes on exit.

### Shutdown

Both shutdown paths use deep sleep, and waking re-runs `setup()` from scratch:

- `engine_onoff()`: PCM volts at or below `ecu_off_volt` for more than 20 consecutive reads. Wakes on the button only.
- `checkCPUTemp()`: at or above `tempOverheat` for more than 10 consecutive reads. Wakes on the button or after 3 minutes. It only reads once `foundOBD2` is set, because the sensor needs the radio active.

### Persistent state

- **NVS namespace `setting`**, opened in `setup()` and left open: `layout` (saved only at auto-off), `showsystem`, `tempOffset`, `ecu_off_volt`, `recent_client`, and one warning value per PID keyed by its `pidConfig` label. Renaming a label orphans the saved value, and labels must stay within NVS's 15-character key limit.
- **NVS namespace `security`**: `serialno`, read by `checkGenuine()`. The call is commented out at `.ino:342`.
- **SPIFFS**: `/TouchCalData1`, `/mypic.jpg` (copied from SD by the menu), `/vaandcob.jpg`. `firmware/partitions.bin` gives SPIFFS 128 KB, despite the scheme's 190 KB label.

### Audio

The speaker on GPIO 26 normally uses LEDC via `ledcWriteTone`. The About page runs `Cartridge` in a core 0 FreeRTOS task, the only multitasking in the firmware, and that player drives the same pin with `dacWrite`. After `vTaskDelete`, `config.h:714` restores the pin and LEDC attachment; any new use of the player needs the same restore.

### Firmware delivery

- `sketch/VaAndCobOBD2Gauge/firmware/` holds release binaries and ESP Web Tools manifests for T5 and T6. The web flasher itself is hosted outside this repo. The `_nogenuine` binaries skip the genuine check, as the current source does.
- On-device updates run from Config menu > Update firmware: `/VaAndCobOBD2Gauge.bin` from the SD root, or a Wi-Fi AP with an on-screen password and an upload page at `192.168.4.1`. Both take the app binary only, which must fit the 1.9 MB OTA slot.
- `gauge_factory_init` writes the serial hard-coded at `gauge_factory_init.ino:14` to NVS, then flashes from SD. It is only needed if `checkGenuine()` is re-enabled.

## Gotchas

- **Line endings:** every text source is committed CRLF except `gauge_factory_init.ino` (LF), with no `.gitattributes`. Keep each file's endings and confirm `git diff --stat` shows only the lines you meant to change. Commit `926ccb3` shows the cost of a flip: about 100k lines of churn with no content change.
- **Hold timers:** `holdtime - millis() > 3000` (`.ino:392`, `.ino:438`, `config.h:333`) subtracts backwards and wraps, so it is true on the second pass. Holds register after one debounce delay (about 200 ms), not the stated 3 s. Swapping the operands changes how every menu feels, so treat it as a UX change.
- **`SKIP_CONNECTION` doesn't skip:** `connectLastOBDII()` still runs at `.ino:383` and resets `foundOBD2` when the connection fails, which lands in the scan loop. Viewing meters without an adapter needs a code change.
- **Fresh flash:** with no `/TouchCalData1`, `touch_calibrate()` blocks `setup()` until all four corners are touched (holding the button at boot forces recalibration). The logo screen then shows a red "File not found" for 3 s, because nothing in this repo writes `/vaandcob.jpg`. Both are expected.
- **Pins:** the display's are in `User_Setup.h`, the touch controller's in `touchscreen.h`, and the rest at the top of the `.ino`, duplicated in `gauge_factory_init.ino`. The button is also hard-coded as `GPIO_NUM_27` at `.ino:222` and `meter.h:434`.
- **Build output:** `.gitignore` only covers `.DS_Store` and some `build/` output is tracked. The IDE's "Export Compiled Binary" writes there, so keep those changes out of commits.
- **Stale comments:** trust the code over the comments. The `.ino` header's line numbers have drifted and it says littleFS where the code uses SPIFFS. The `pidInCell` comment and parts of `engine_onoff()` say auto-off uses engine speed, but it checks PCM volts.
