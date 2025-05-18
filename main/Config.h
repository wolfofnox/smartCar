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
#pragma endregion

// --- CONFIG ---
// Application-wide configuration macros and feature toggles
#pragma region Config

#define LOG_LEVEL_GLOBAL ESP_LOG_INFO   ///< Set the global log level for esp-idf components
#define LOG_LEVEL_SOURCE ESP_LOG_DEBUG   ///< Set the log level for this source file

#define USE_NVS            0  ///< Use NVS (Non-Volatile Storage) for settings (1/0 = yes/no)
#if USE_NVS == 1
    #include "nvs_flash.h"
    #include "nvs.h"
    #define NVS_NAMESPACE_APP "app_settings"   ///< NVS namespace for app settings
    #define NVS_NAMESPACE_USER "user_settings" ///< NVS namespace for user settings
#endif

#define USE_WiFi             1  ///< Use WiFi in station mode (1/0 = yes/no)
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