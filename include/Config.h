#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>


// --- Pin Config ---
#define PIN_3V3_BUS 47
#define LED 45
#define M1 41
#define M2 40
#define M_EN 39
#define STEER_SERVO 38
#define TOP_SERVO 37

// --- CONFIG ---
#define BAUDRATE         115200  // Baudrate for Hardware Serial

#define USE_EEPROM            0  // Use EEPROM for settings (1/0 = yes/no)
#ifdef USE_EEPROM
#define EEPROM_PROJECT_ID  0x00
#endif

#define USE_WiFi             1  // Use WiFi in station mode (1/0 = yes/no)
#define USE_mDNS             1  // Use mDNS (1/0 = yes/no)
#define USE_SD_SERVER        0  // Use SD card (1/0 = yes/no)
#if USE_WiFi == 1
#define MY_SSID        "O2-Internet-826" // SSID of the wifi network
#define MY_PASS        "bA6RfeFT"        // Password of the wifi network
#define STATIC_IP   IPAddress(10,0,0,58)
#ifdef STATIC_IP
#define GATEWAY     IPAddress(192,168,1,1)
#define SUBNET      IPAddress(255,255,255,0)
#define HOSTNAME   "ESP"      // For router discovery, only alfanumeric + '-'
#endif
#if USE_mDNS == 1
#define mDNS_HOSTNAME   "esp"
#define SERVICE_NAME     "esp"
#endif
#endif

// --- Board specific ---
#pragma region BoardSpecificDefines
#if not  defined(ESP32)
#error "Unsupported platform!\n Please use ESP32"
#endif

#if USE_EEPROM == 1
#include <EEPROM.h>
#endif

#pragma endregion
// ---

// --- Class, Enum and Struct definitions ---
#pragma region ClassEnumStructs

#pragma endregion

#endif // CONFIG_H