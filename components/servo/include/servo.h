#pragma once

#include "driver/mcpwm_prelude.h"
#include "esp_log.h"
#include "esp_check.h"

typedef struct servo_t *servo_handle_t;

typedef struct {
    uint32_t min_pulsewidth_us;
    uint32_t max_pulsewidth_us;
    int8_t min_degree; // default -90
    int8_t max_degree; // default 90
    uint32_t resolution_hz;
    uint32_t period_ticks;
    int gpio_num;
} servo_config_t;

servo_handle_t servo_init(servo_config_t *config);
esp_err_t servo_set_angle(servo_handle_t servo, int8_t angle);
esp_err_t servo_set_nim_max_degree(servo_handle_t servo, int8_t min_angle, int8_t max_angle);
esp_err_t servo_set_nim_max_pulsewidth(servo_handle_t servo, int32_t min_pulsewidth_us, int32_t max_pulsewidth_us);
int8_t servo_get_angle(servo_handle_t servo);
esp_err_t servo_deinit(servo_handle_t servo);
