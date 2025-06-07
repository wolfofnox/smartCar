#include "l298n_motor.h"
#include <stdlib.h>

typedef struct {
    gpio_num_t in1_pin;
    gpio_num_t in2_pin;
    gpio_num_t en_pin;

    ledc_channel_t ledc_channel;
    ledc_timer_t ledc_timer;
    ledc_mode_t ledc_mode;

    uint32_t pwm_max_duty;

    int8_t speed;
} l298n_motor_t;

l298n_motor_handle_t l298n_motor_init(const l298n_motor_config_t *config) {
    const char *TAG = "l298n_motor_init";
    if (!config) return NULL;

    l298n_motor_t *motor = calloc(1, sizeof(l298n_motor_t));
    if (!motor) {
        ESP_LOGE(TAG, "failed to allocate motor struct");
        return NULL;
    }

    motor->in1_pin = config->in1_pin;
    motor->in2_pin = config->in2_pin;
    motor->en_pin = config->en_pin;

    motor->ledc_channel = config->ledc_channel;
    motor->ledc_timer = config->ledc_timer;
    motor->ledc_mode = config->ledc_mode;

    motor->speed = 0;

    // Configure GPIOs for direction pins
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << motor->in1_pin) | (1ULL << motor->in2_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    if (gpio_config(&io_conf) != ESP_OK) {
        ESP_LOGE(TAG, "failed to set gpio config");
        free(motor);
        return NULL;
    }

    // Configure LEDC timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode = motor->ledc_mode,
        .timer_num = motor->ledc_timer,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz = config->pwm_freq_hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    if (ledc_timer_config(&ledc_timer) != ESP_OK) {
        ESP_LOGE(TAG, "failed to set ledc timer config");
        free(motor);
        return NULL;
    }

    // Configure LEDC channel for EN pin PWM
    ledc_channel_config_t ledc_channel = {
        .gpio_num = motor->en_pin,
        .speed_mode = motor->ledc_mode,
        .channel = motor->ledc_channel,
        .timer_sel = motor->ledc_timer,
        .duty = 0,
        .hpoint = 0,
    };
    if (ledc_channel_config(&ledc_channel) != ESP_OK) {
        free(motor);
        return NULL;
    }

    motor->pwm_max_duty = (1 << LEDC_TIMER_13_BIT) - 1;

    // Init motor stopped
    ESP_RETURN_ON_ERROR(gpio_set_level(motor->in1_pin, 0), TAG, "failed to set gpio %d", motor->in1_pin);
    ESP_RETURN_ON_ERROR(gpio_set_level(motor->in2_pin, 0), TAG, "failed to set gpio %d", motor->in2_pin);
    ledc_set_duty(motor->ledc_mode, motor->ledc_channel, 0);
    ledc_update_duty(motor->ledc_mode, motor->ledc_channel);

    return (l298n_motor_handle_t)motor;
}

esp_err_t l298n_motor_set_speed(l298n_motor_handle_t motor, int8_t speed_percent) {
    const char *TAG = "l298n_motor_set_speed";
    if (!motor) return ESP_ERR_INVALID_ARG;
    l298n_motor_t *mtr = (l298n_motor_t *)motor;

    if (speed_percent > 100) speed_percent = 100;
    if (speed_percent < -100) speed_percent = -100;

    if (speed_percent == 0) {
        // brake: both direction pins low, PWM off
        ESP_RETURN_ON_ERROR(gpio_set_level(mtr->in1_pin, 0), TAG, "failed to set gpio %d", mtr->in1_pin);
        ESP_RETURN_ON_ERROR(gpio_set_level(mtr->in2_pin, 0), TAG, "failed to set gpio %d", mtr->in2_pin);
        ledc_set_duty(mtr->ledc_mode, mtr->ledc_channel, 0);
        return ledc_update_duty(mtr->ledc_mode, mtr->ledc_channel);
    }

    // Set direction pins accordingly
    if (speed_percent > 0) {
        ESP_RETURN_ON_ERROR(gpio_set_level(mtr->in1_pin, 1), TAG, "failed to set gpio %d", mtr->in1_pin);
        ESP_RETURN_ON_ERROR(gpio_set_level(mtr->in2_pin, 0), TAG, "failed to set gpio %d", mtr->in2_pin);
    } else {
        ESP_RETURN_ON_ERROR(gpio_set_level(mtr->in1_pin, 0), TAG, "failed to set gpio %d", mtr->in1_pin);
        ESP_RETURN_ON_ERROR(gpio_set_level(mtr->in2_pin, 1), TAG, "failed to set gpio %d", mtr->in2_pin);
        speed_percent = -speed_percent;
    }

    // Calculate duty
    uint32_t duty = (mtr->pwm_max_duty * speed_percent) / 100;

    ledc_set_duty(mtr->ledc_mode, mtr->ledc_channel, duty);
    return ledc_update_duty(mtr->ledc_mode, mtr->ledc_channel);
}

esp_err_t l298n_motor_stop(l298n_motor_handle_t motor) {
    return l298n_motor_set_speed(motor, 0);
}

int8_t l298n_motor_get_speed(l298n_motor_handle_t motor) {
    l298n_motor_t *mtr = (l298n_motor_t *)motor;
    return mtr->speed;
}

esp_err_t l298n_motor_deinit(l298n_motor_handle_t motor) {
    const char *TAG = "l298n_motor_deinit";
    if (!motor) return ESP_ERR_INVALID_ARG;
    l298n_motor_t *mtr = (l298n_motor_t *)motor;

    // Stop motor
    l298n_motor_stop(motor);

    ESP_RETURN_ON_ERROR(ledc_stop(mtr->ledc_mode, mtr->ledc_channel, 0), TAG, "failed to stop ledc");

    // Optionally reset GPIOs (not mandatory)
    ESP_RETURN_ON_ERROR(gpio_set_level(mtr->in1_pin, 0), TAG, "failed to set gpio %d", mtr->in1_pin);
    ESP_RETURN_ON_ERROR(gpio_set_level(mtr->in2_pin, 0), TAG, "failed to set gpio %d", mtr->in2_pin);
    gpio_reset_pin(mtr->in1_pin);
    gpio_reset_pin(mtr->in2_pin);

    // Reset PWM pin
    gpio_reset_pin(mtr->en_pin);

    free(mtr);
    return ESP_OK;
}

l298n_motor_config_t *l298n_motor_get_config(l298n_motor_handle_t motor) {
    if (!motor) return NULL;
    l298n_motor_t *mtr = (l298n_motor_t *)motor;

    l298n_motor_config_t *config = calloc(1, sizeof(l298n_motor_config_t));
    if (!config) return NULL;

    config->in1_pin = mtr->in1_pin;
    config->in2_pin = mtr->in2_pin;
    config->en_pin = mtr->en_pin;
    config->ledc_channel = mtr->ledc_channel;
    config->ledc_timer = mtr->ledc_timer;
    config->ledc_mode = mtr->ledc_mode;
    config->pwm_freq_hz = ledc_get_freq(mtr->ledc_mode, mtr->ledc_timer);

    return config;
}
