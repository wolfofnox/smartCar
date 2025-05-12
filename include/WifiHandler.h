#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include <Arduino.h>
#include <Config.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#if USE_mDNS == 1
#include <ESPmDNS.h>
#endif
#include <WiFiClient.h>
#if USE_SD_SERVER == 1
#include <SD.h>
#include <SPI.h>
#endif 
#include <FastLED.h>

extern AsyncWebServer Server; // Create a web server on port 80
extern AsyncWebSocket ws; // Create a WebSocket server on "/ws"

extern void steer(int value);
extern void drive(int value);
extern void topServoGo(int value);

extern int speed;
extern int steering;
extern int top;

#if USE_SD_SERVER == 1
extern SDFS SD;
#endif

extern ulong bootTime;
extern bool ledOn;
extern CRGB leds[1]; // Array for the RGB LED

#if USE_SD_SERVER == 1
void sd_init();
#else
inline void sd_init() {
  log_i("SD card not used.");
}
#endif
void wifi_init();
void set_callbacks();

void handle404(AsyncWebServerRequest *request);

#endif