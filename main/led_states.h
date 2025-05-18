#ifndef LED_STATES_H
#define LED_STATES_H

#pragma once
#include "led_indicator.h"

enum {
    BLINK_OFF = 0,
    BLINK_LOADING,
    BLINK_LOADED,
    BLINK_WIFI_CONNECTING,
    BLINK_WIFI_CONNECTED,
    BLINK_WIFI_DISCONNECTED,
    BLINK_WIFI_AP_STARTING,
    BLINK_WIFI_AP_STARTED,
    BLINK_MAX
};

static const blink_step_t off[] = {
    {LED_BLINK_HOLD, LED_STATE_OFF, 0},
    {LED_BLINK_STOP, 0, 0}
};

static const blink_step_t loading[] = {
    {LED_BLINK_HSV, SET_HSV(0, 0, 0), 0},
    {LED_BLINK_BREATHE, LED_STATE_75_PERCENT, 500},
    {LED_BLINK_BREATHE, LED_STATE_OFF, 500},
    {LED_BLINK_LOOP, 0, 0}
};

static const blink_step_t loaded[] = {
    {LED_BLINK_HSV, SET_HSV(0, 0, 0), 0},
    {LED_BLINK_HOLD, LED_STATE_OFF, 100},
    {LED_BLINK_HOLD, LED_STATE_ON, 100},
    {LED_BLINK_HOLD, LED_STATE_OFF, 100},
    {LED_BLINK_HOLD, LED_STATE_ON, 100},
    {LED_BLINK_HOLD, LED_STATE_OFF, 100},
    {LED_BLINK_STOP, 0, 0}
};

static const blink_step_t wifi_connecting[] = {
    {LED_BLINK_HSV, SET_HSV(40, MAX_SATURATION, 0), 0},
    {LED_BLINK_BREATHE, LED_STATE_75_PERCENT, 500},
    {LED_BLINK_BREATHE, LED_STATE_OFF, 500},
    {LED_BLINK_LOOP, 0, 0}
};

static const blink_step_t wifi_connected[] = {
    {LED_BLINK_HSV, SET_HSV(40, MAX_SATURATION, 0), 0},
    {LED_BLINK_HOLD, LED_STATE_OFF, 100},
    {LED_BLINK_HOLD, LED_STATE_ON, 100},
    {LED_BLINK_HOLD, LED_STATE_OFF, 100},
    {LED_BLINK_HOLD, LED_STATE_ON, 100},
    {LED_BLINK_HOLD, LED_STATE_OFF, 100},
    {LED_BLINK_STOP, 0, 0}
};

static const blink_step_t wifi_disconnected[] = {
    {LED_BLINK_HSV, SET_HSV(0, MAX_SATURATION, 0), 0},
    {LED_BLINK_HOLD, LED_STATE_OFF, 100},
    {LED_BLINK_HOLD, LED_STATE_ON, 100},
    {LED_BLINK_HOLD, LED_STATE_OFF, 100},
    {LED_BLINK_HOLD, LED_STATE_ON, 100},
    {LED_BLINK_HOLD, LED_STATE_OFF, 100},
    {LED_BLINK_HOLD, LED_STATE_ON, 100},
    {LED_BLINK_HOLD, LED_STATE_OFF, 100},
    {LED_BLINK_STOP, 0, 0}
};

static const blink_step_t wifi_ap_starting[] = {
    {LED_BLINK_HSV, SET_HSV(210, MAX_SATURATION, 0), 0},
    {LED_BLINK_BREATHE, LED_STATE_75_PERCENT, 500},
    {LED_BLINK_BREATHE, LED_STATE_OFF, 500},
    {LED_BLINK_LOOP, 0, 0}
};

static const blink_step_t wifi_ap_started[] = {
    {LED_BLINK_HSV, SET_HSV(210, MAX_SATURATION, 0), 0},
    {LED_BLINK_HOLD, LED_STATE_OFF, 100},
    {LED_BLINK_HOLD, LED_STATE_ON, 100},
    {LED_BLINK_HOLD, LED_STATE_OFF, 100},
    {LED_BLINK_HOLD, LED_STATE_ON, 100},
    {LED_BLINK_HOLD, LED_STATE_OFF, 100},
    {LED_BLINK_STOP, 0, 0}
};

extern const blink_step_t *led_blink_list[];

#endif