#pragma once

#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"

typedef struct l298n_motor_t *l298n_motor_handle_t;

typedef struct {
    gpio_num_t in1_pin;
    gpio_num_t in2_pin;
    gpio_num_t en_pin;

    ledc_channel_t ledc_channel;  // PWM channel for EN
    ledc_timer_t ledc_timer;      // PWM timer
    ledc_mode_t ledc_mode;        // PWM speed mode

    uint32_t pwm_freq_hz;         // PWM frequency, e.g. 5000
    
    // Rotary encoder pins
    gpio_num_t encoder_a_pin;
    gpio_num_t encoder_b_pin;
    uint16_t encoder_pulses_per_rev; // Pulses per revolution
} l298n_motor_config_t;

esp_err_t l298n_motor_init(l298n_motor_handle_t *motor, const l298n_motor_config_t *config);
esp_err_t l298n_motor_set_speed(l298n_motor_handle_t motor, int8_t speed_percent);
esp_err_t l298n_motor_stop(l298n_motor_handle_t motor);
int8_t l298n_motor_get_speed(l298n_motor_handle_t motor);
esp_err_t l298n_motor_deinit(l298n_motor_handle_t motor);
l298n_motor_config_t *l298n_motor_get_config(l298n_motor_handle_t motor);

// Rotary encoder/angle functions
esp_err_t l298n_motor_reset_angle(l298n_motor_handle_t motor);
float l298n_motor_get_angle(l298n_motor_handle_t motor);
esp_err_t l298n_motor_drive_to_angle(l298n_motor_handle_t motor, float target_angle, int8_t speed_percent);