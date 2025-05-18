#ifndef CONFIG_H
#define CONFIG_H

// --- Includes ---
// Standard and ESP-IDF includes for core functionality
#pragma region Includes
#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#include "servo.h"
#include "led_states.h"
#pragma endregion

// --- Pin Config ---
// Pin assignments for hardware peripherals
#pragma region PinConfig
#define PIN_3V3_BUS 47      ///< 3.3V bus enable pin
#define PIN_LED 45          ///< On-board rgb LED pin
#define PIN_SD_CS 10        ///< SD card chip select pin (uŠup)
#define PIN_SPI_SCK 12      ///< SPI uŠup clock pin (SPI2)
#define PIN_SPI_MOSI 11     ///< SPI uŠup MOSI pin (SPI2)
#define PIN_SPI_MISO 13     ///< SPI uŠup MISO pin (SPI2)
#define PIN_MOT_1 37
#define PIN_MOT_2 36
#define PIN_MOT_EN 38
#define PIN_STEER_SERVO 48
#define PIN_TOP_SERVO 21
#define PIN_BAT_VOLT 8     ///< Battery voltage devider pin
#pragma endregion

// --- CONFIG ---
// Application-wide configuration macros and feature toggles
#pragma region Config

#define LOG_LEVEL_GLOBAL ESP_LOG_INFO   ///< Set the global log level for esp-idf components
#define LOG_LEVEL_SOURCE ESP_LOG_DEBUG   ///< Set the log level for this source file

// Please consult the datasheet of your servo before changing the following parameters
#define SERVO_MIN_PULSEWIDTH_US 500  // Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH_US 2500  // Maximum pulse width in microsecond
#define SERVO_MIN_DEGREE        -90   // Minimum angle
#define SERVO_MAX_DEGREE        90    // Maximum angle

#define SERVO_PULSE_GPIO             0        // GPIO connects to the PWM signal line
#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms

#define BATTERY_VOLTAGE_DEV_R1 9985.0 // 10k
#define BATTERY_VOLTAGE_DEV_R2 39400.0 // 40k
#define BATTERY_VOLTAGE_MULTIPLIER ((BATTERY_VOLTAGE_DEV_R1 + BATTERY_VOLTAGE_DEV_R2) / BATTERY_VOLTAGE_DEV_R1) // ~5.0

#define BATTERY_WALL_ADAPTER 0
#define BATTERY_6xNiMH 1 // 6 NiMH rechargeable 1.2V batteries (1V - 1.5V per cell, 6V - 9.0V total)

#define BATTERY_TYPE BATTERY_6xNiMH // from one of the above 

#define USE_NVS            0  ///< Use NVS (Non-Volatile Storage) for settings (1/0 = yes/no)
#if USE_NVS == 1
    #include "nvs_flash.h"
    #include "nvs.h"
    #define NVS_NAMESPACE_APP "app_settings"   ///< NVS namespace for app settings
    #define NVS_NAMESPACE_USER "user_settings" ///< NVS namespace for user settings
#endif

#define USE_WiFi             0  ///< Use WiFi in station mode (1/0 = yes/no)
#define USE_mDNS             1  ///< Use mDNS (1/0 = yes/no)
#define USE_SD_SERVER        0  ///< Use SD card (1/0 = yes/no)
#if USE_WiFi == 1
    // If WiFi is enabled, force NVS usage and include WiFi header
    #undef USE_NVS
    #define USE_NVS 1
    #include "Wifi.h"
    #define NVS_NAMESPACE_WIFI "wifi_settings" ///< NVS namespace for WiFi settings

    #define MAX_STA_FAILS  5          ///< Max number of failed attempts to connect to the AP before switching to AP captive mode
#endif
#pragma endregion

// --- Class, Enum and Struct definitions ---
// (Reserved for future class, enum, or struct definitions)
#pragma region ClassEnumStructs

#pragma endregion

#endif