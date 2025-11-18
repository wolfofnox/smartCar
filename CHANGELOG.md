# Changelog

All notable changes to the ESP32 smartCar project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [v0.1.1] - 2025-11-18

### Added

- CHANGELOG.md (this file) and [README.md](https://github.com/wolfofnox/smartCar/blob/main/README.md)

### Changed

- Updated Wifi and Captive portal manager to official releases (v0.2.1) from [Github](https://github.com/wolfofnox/esp32-captive-wifi-manager)
  - Added better SSID selection and captive portal UI
  - Added support for OPEN networks
  - Added a AP-only mode

## [v0.1.0] - 2025-10-20

Basic, "barely working" MVP

### Features

- Drive, steering
- Steering calibration on the webpage
- Captive portal connecting to WPA2 wifi networks
- Remote control over a webserver using websocket comunications
- Battery voltage meassurement and safety shutoff (for  6-cell NiMH)

### Expected Hardware

- Rear wheel drive, one motor, l298n motor controller
- Front wheel steering using a servo
