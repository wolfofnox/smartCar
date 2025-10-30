# SmartCar — README

Project: ESP32-S3 SmartCar  

## Overview
SmartCar is a small ESP32-S3–based robotic car running on ESP-IDF. It supports differential drive, obstacle avoidance, line following, and remote control (Wi‑Fi / UART). This README covers hardware, wiring, building with ESP-IDF, flashing, calibration, and troubleshooting.

### Important - WIP
The projeczt is stil in early beta, any upadate might be braking, any existing feature might be changed or removed.
Use this code at your own risk.

## Features
- Differential drive with two DC motors (L298N or similar)
- Remote control via Wi‑Fi
- Battery-powered; configurable speed and behaviors

---

## Install (ESP-IDF)
1. Install ESP-IDF (follow official guide): https://docs.espressif.com/projects/esp-idf
2. Open a shell and source the ESP-IDF environment (idf.py requires this).
3. Clone or open this project folder.
4. Set target to esp32s3:
    - idf.py set-target esp32s3
5. Configure project:
    - idf.py menuconfig — set serial port, partition table, PWM timers, ADC attenuation, Wi‑Fi/BLE settings
6. Build and flash:
    - idf.py build
    - idf.py -p COMx flash monitor
    Replace COMx with your board port. Use idf.py monitor to view logs.

---

## Install (PlatformIO — optional)
If you prefer PlatformIO, use an env with framework = espidf:

Example platformio.ini excerpt:
```
[env:esp32s3]
platform = espressif32
board = esp32s3_dev
framework = espidf
```
Use PlatformIO build/upload or pio run --target upload.

---

## Safety notes
- Verify voltage and current ratings for sensors and the ESP32-S3.
- Do not power 5V sensors directly from ESP32 pins; use proper regulators/level shifters.
- Avoid handling wheels while motors are powered.

---

## Contributing
Focus contributions on clear issue reports and bug tracking before submitting PRs.

When reporting an issue, provide:
- Short title and severity
- Hardware: board model, motor driver, sensors, battery voltage
- Firmware: git commit hash / release tag, branch
- Steps to reproduce (minimal steps)
- Expected vs actual behavior
- Logs: idf.py monitor output, serial logs, relevant stack traces
- Attach: photos of wiring, schematic, and minimum reproducible code or configuration (sdkconfig.defaults or menuconfig snapshot)
- Any debugging you tried and results

Preferred workflow:
- Open an issue with the above information.
- Maintainers will request additional data or a small PR with a fix.
- For code changes: branch from main, include descriptive commit messages, and reference the issue in the PR.

---

## License
Under Apache 2.0. See LICENSE file

---
