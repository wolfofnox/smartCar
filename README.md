# SmartCar — README

Project: ESP32-S3 SmartCar  

## Overview
SmartCar is a small ESP32-S3–based robotic car running on ESP-IDF. It features rear-wheel drive with steering control and remote operation via Wi-Fi WebSocket interface.

### Important - WIP
The project is still in early beta. Any update might be breaking, and any existing feature might be changed or removed.
Use this code at your own risk.

## Features
- Rear-wheel drive with single DC motor (L298N motor driver)
- Front-wheel steering with servo
- Motor encoder support for angle tracking
- Remote control via Wi-Fi WebSocket
- Battery voltage monitoring with safety shutoff (6-cell NiMH)
- OLED display (SSD1306) for status information
- Web-based calibration interface
- Captive portal for Wi-Fi configuration

---

## Hardware Requirements
- ESP32-S3 development board
- L298N motor driver
- DC motor with encoder (for rear-wheel drive)
- Servo motor (for steering)
- SSD1306 OLED display (128x64, I2C)
- 6-cell NiMH battery pack (or similar)
- Voltage divider for battery monitoring (configured via menuconfig)

---

## Installation

### Prerequisites
1. Install ESP-IDF v5.0 or later (follow the [official installation guide](https://docs.espressif.com/projects/esp-idf))
2. Open a terminal and activate the ESP-IDF environment:
   ```bash
   . $HOME/esp/esp-idf/export.sh
   ```

### Building and Flashing
1. Clone this repository:
   ```bash
   git clone https://github.com/wolfofnox/smartCar.git
   cd smartCar
   ```

2. Set the target to ESP32-S3:
   ```bash
   idf.py set-target esp32s3
   ```

3. Configure the project (adjust pins, ADC settings, Wi-Fi options, etc.):
   ```bash
   idf.py menuconfig
   ```

4. Build the project:
   ```bash
   idf.py build
   ```

5. Flash to your board and monitor output:
   ```bash
   idf.py -p /dev/ttyUSB0 flash monitor
   ```
   Replace `/dev/ttyUSB0` with your board's serial port (e.g., `COM3` on Windows).

---

## Configuration
Use `idf.py menuconfig` to configure:
- **Pin Configuration**: Motor pins, servo pins, I2C pins, ADC channel
- **Voltage Divider**: R1 and R2 values for battery voltage measurement
- **Motor Settings**: PWM frequency, encoder pulses per revolution
- **Logging**: Set log levels for debugging

---

## Usage
1. Power on the SmartCar
2. Connect to the captive portal Wi-Fi network broadcasted by the ESP32-S3
3. Configure your Wi-Fi network through the captive portal
4. Once connected, access the web interface at the ESP32's IP address
5. Use the web interface to:
   - Control the car (drive, steer)
   - Calibrate the steering servo
   - Monitor battery voltage and status

---

## Safety Notes
- Verify voltage and current ratings for all components before connecting
- Do not power 5V sensors directly from ESP32 pins; use appropriate level shifters or regulators
- Ensure proper battery polarity and voltage before connecting
- Keep hands away from wheels and moving parts while motors are powered
- Use appropriate battery protection (over-discharge, over-current)

---

## Contributing
Focus contributions on clear issue reports and bug tracking before submitting PRs.

When reporting an issue, provide:
- **Title**: Short, descriptive title with severity indication
- **Hardware**: Board model, motor driver, sensors, battery voltage
- **Firmware**: Git commit hash/release tag, branch name
- **Steps to reproduce**: Minimal steps to trigger the issue
- **Expected vs actual behavior**: What should happen vs what actually happens
- **Logs**: Output from `idf.py monitor`, relevant stack traces
- **Attachments**: Photos of wiring, schematics, configuration files (sdkconfig.defaults)
- **Debugging attempts**: What you've tried and the results

Preferred workflow:
1. Open an issue with the above information
2. Maintainers will review and may request additional data or suggest fixes
3. For code changes: branch from main, use descriptive commit messages, reference the issue number in your PR

---

## License
Licensed under Apache 2.0. See the LICENSE file for details.

---
