# Stait Watch - ESP32 Firmware

Stait Watch is an open-source firmware for the **ESP32-S3** platform, specifically designed for Waveshare ESP32-S3 1.28" Touch Display. It enables real-time interaction with an Android companion app via Bluetooth Low Energy (BLE), managing notifications, weather, and system settings.

## 📋 Project Overview
The firmware is built using **LovyanGFX** for high-performance graphics and **BLE-Arduino** for connectivity. It features a custom, linguistically neutral protocol using numeric codes, making it easy to adapt to any UI language. 

The interface is completely gesture-based, allowing you to swipe between the watchface, media controls, notifications, weather forecasts, and system settings.

## 🛠 Technical Features
* **High-Performance UI**: Smooth animations and graphics powered by LovyanGFX.
* **Gesture Control**: Intuitive swipe navigation and tap-to-wake functionality.
* **BLE Connectivity**: Stable communication with Android devices for data synchronization.
* **Weather-as-a-Code**: Uses universal codes for weather states, ensuring compatibility without firmware-side translations.
* **System Management**: Built-in settings for screen brightness, timeout, and "Do Not Disturb" (DND) modes.

## ⚠️ Important Status & Known Issues
* **Battery Monitoring**: The battery percentage calculation is **currently under development and not 100% stable**. You may notice fluctuations in the reported percentage; further calibration for the specific voltage divider/ADC readings is ongoing.
* **Compatibility**: This code has been tested and optimized **exclusively for the Waveshare ESP32-S3 1.28" Touch Display**. Using it on different hardware layouts will require modifications to the main code.

## 🚀 Companion App & Ecosystem
This firmware works in tandem with **Stait Watch App**, the official Android companion application developed in Flutter. Stait Watch App handles system-level notification filtering, background connectivity, and weather data pushing.

* **App Repository**: [Insert Link to Stait Hub Repository Here]

## 💻 Configuration and Compilation
This project is developed to be compiled using **Arduino IDE**.

### Required Libraries
Ensure you have installed the following via your Library Manager:
- **LovyanGFX**: High-performance graphics driver.
- **BLE-Arduino**: Standard ESP32 BLE stack.
- **ArduinoJson**: Efficient message parsing.

### Setup
1. Clone this repository: `git clone https://github.com/SantoCovato/Stait-Watch.git`
2. Ensure your environment is configured for the **ESP32-S3** board.
3. Compile and upload the firmware.

## 📡 Communication Protocol
The firmware expects strings from the Android app using these prefixes:
* `DT:` (Time Synchronization)
* `W0:` (Current Weather: Temperature,Code)
* `W1-W3:` (Forecasts)
* `NT:` (Notifications: Sender|Message)

## 🤝 Contributing
Contributions are highly encouraged! If you have suggestions for stabilizing the battery ADC readings or want to add support for new displays, feel free to open a Pull Request.

## 📝 License
This project is licensed under the **Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)** license. 

This means you are free to share and adapt the material for personal use, provided that you give appropriate credit to the author, but you **may not use this material for commercial purposes.**

---
*Firmware developed for the Stait Watch system.*
