# Stait Watch - ESP32 Firmware

[![License: CC BY-NC 4.0](https://img.shields.io/badge/License-CC_BY--NC_4.0-lightgrey.svg)](https://creativecommons.org/licenses/by-nc/4.0/)

Stait Watch is an open-source firmware for the **ESP32-S3** platform, specifically designed for the Waveshare ESP32-S3 Round LCD Touch 1.28-inch display. It provides a gesture-based smartwatch interface and communicates with a private Android companion app through Bluetooth Low Energy (BLE).

**Current firmware version: v1.1.0**

## 📸 Interface

The firmware provides five main screens:

* **Watchface**
* **Notifications**
* **Weather**
* **Settings**
* **Media Player**
* **Incoming calls**

## 📋 Project Overview

This public repository contains the **ESP32 firmware only**. The Android companion application is developed separately with Flutter and its source code is not included in this repository.

The firmware is built using **LVGL** for the user interface, **LovyanGFX** for the display and touch hardware, and **NimBLE-Arduino** for Bluetooth Low Energy communication.

The private companion app sends structured binary messages to synchronize the watch. The protocol uses numeric message codes and does not depend on a specific UI language. The firmware supports English and Italian; Italian is selected when the phone locale is `it`, while other locales fall back to English.

## 🛠 Technical Features

* **LVGL interface**: Watchface, notifications, weather, settings, media player, and incoming-call screens.
* **Gesture control**: Swipe navigation and touch-to-wake behavior.
* **BLE synchronization**: Date/time, weather, forecasts, notifications, language, media state, and incoming calls.
* **Italian localization**: Italian translations with accented characters such as `à`, `è`, `ì`, `ò`, and `ù`.
* **Monochrome emoji**: Generated Montserrat/Noto Emoji LVGL fonts for 12, 14, and 16 pixel text.
* **Weather icons**: Custom 48 pixel font containing the required weather symbols and moon icon.
* **Day/night weather mode**: A sunny icon changes to a moon from 19:00 through 06:59.
* **Notification hierarchy**: App name, time, group/person, and message are displayed with separate visual levels.
* **Media title behavior**: Long titles scroll horizontally; short titles are centered automatically.
* **Call screen**: Incoming-call display with caller name and reject action.
* **System settings**: Brightness, screen timeout, 12/24-hour format, notifications, and BLE unpairing.
* **BLE MAC display**: The settings screen shows the NimBLE Bluetooth address used by the app, rather than the Wi-Fi MAC address.
* **Hidden scrollbars**: Scrollbars are hidden while notification list scrolling remains available.

## ⚠️ Important Status & Known Issues

* **Battery Monitoring**: The battery percentage calculation depends on the board voltage divider and ADC calibration. Readings may require further calibration for the specific hardware.
* **Compatibility**: This code has been tested and optimized for the Waveshare ESP32-S3 Round LCD Touch 1.28-inch board with a 240x240 GC9A01 display and CST816S touch controller.

## 🚀 Companion App & Ecosystem

This firmware works together with **Stait Watch App**, a private Flutter Android companion application.

The companion app handles:

- BLE scanning and connection
- Notification filtering and forwarding
- Weather data and forecasts
- System language synchronization
- Media controls and playback state
- Incoming-call events
- Background BLE connectivity through an Android foreground service

The current app release is **v1.1.0+2** and targets **Android API 36**. The `+2` value is the Android build number used for Google Play releases.

* **Companion App Repository**: [https://github.com/SantoCovato/Stait-Watch-App](https://github.com/SantoCovato/Stait-Watch-App)

## 💻 Configuration and Compilation

This firmware is compiled using **PlatformIO**, either through the PlatformIO VS Code extension or the PlatformIO CLI.

### Required Software

Ensure that the following are available:

- PlatformIO
- ESP32 Arduino framework
- LVGL 9.x
- LovyanGFX
- NimBLE-Arduino

### Setup

1. Clone the firmware repository:

```bash
git clone https://github.com/SantoCovato/Stait-Watch.git
```

2. Open the repository with VS Code and install the PlatformIO dependencies.
3. Connect the Waveshare ESP32-S3 board.
4. Build and upload the `waveshare_esp32s3` environment.

### Build

From the repository root:

```powershell
Push-Location main
pio run -e waveshare_esp32s3
Pop-Location
```

### Upload

Replace `COM8` with the serial port of the board when necessary:

```powershell
Push-Location main
pio run -e waveshare_esp32s3 -t upload --upload-port COM8
Pop-Location
```

## 📡 Communication Protocol

The firmware expects binary BLE messages from the companion app using the following codes:

* `0x01` (`DT`): Date and time synchronization
* `0x02` (`W0`): Current weather
* `0x03` (`W1-W3`): Weather forecast
* `0x04` (`NT`): Notification
* `0x05`: System language
* `0x06`: Incoming call
* `0x07`: Call ended
* `0x08`: Media playback state

The watch sends control commands to the app:

* `0x10`: Play/pause
* `0x11`: Next track
* `0x12`: Previous track
* `0x13`: Reject incoming call

### BLE UUIDs

**Service UUID**

```text
4FAFC201-1FB5-459E-8FCC-C5C9C331914B
```

**Characteristic UUID**

```text
BEA56A26-34EF-44B4-A36F-272A7762AF31
```

Weather states use language-neutral values such as `SUN`, `CLOUDS`, `RAIN`, `SNOW`, and `STORM`.

## 🧩 Firmware Files

The repository is based on the contents of the `main` directory:

- `main.ino`: Hardware initialization, BLE communication, and application entry point
- `ui.c` and `ui.h`: LVGL screens and UI updates
- `lv_conf.h`: LVGL configuration
- `Montserrat_custom_12.c`, `Montserrat_custom_14.c`, `Montserrat_custom_16.c`: Fonts with accents, symbols, and monochrome emoji
- `Montserrat_custom_48.c`: Large weather font with weather symbols and moon
- `Staitwatchface.c`: ARGB8888 logo image with transparency
- `platformio.ini`: PlatformIO build configuration

## 🤝 Contributing

Contributions are welcome. Useful areas include battery ADC calibration, additional display support, UI refinement, power optimization, and BLE reliability.

## 📝 License

This project is licensed under the Creative Commons Attribution-NonCommercial 4.0 International license (CC BY-NC 4.0).

This means you are free to share and adapt the firmware for personal and non-commercial purposes, provided that appropriate credit is given. Commercial use is not permitted under this license.

For the full license terms, see:

https://creativecommons.org/licenses/by-nc/4.0/

---

*Firmware developed for the Stait Watch system.*
