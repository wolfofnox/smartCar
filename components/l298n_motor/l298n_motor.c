#include "l298n_motor.h"
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

// Forward declaration for rotary encoder ISR
static void l298n_motor_encoder_isr(void *arg);

typedef struct {
    gpio_num_t in1_pin;
    gpio_num_t in2_pin;
    gpio_num_t en_pin;

    ledc_channel_t ledc_channel;
    ledc_timer_t ledc_timer;
    ledc_mode_t ledc_mode;

    uint32_t pwm_max_duty;

    int8_t speed;
    
    // Rotary encoder
    gpio_num_t encoder_a_pin;
    gpio_num_t encoder_b_pin;
    uint16_t encoder_pulses_per_rev;
    volatile int32_t encoder_count;
} l298n_motor_t;

esp_err_t l298n_motor_init(l298n_motor_handle_t *motor, const l298n_motor_config_t *config) {
    const char *TAG = "l298n_motor_init";
    if (!config) return ESP_ERR_INVALID_ARG;

    l298n_motor_t *mtr = calloc(1, sizeof(l298n_motor_t));
    if (!mtr) {
        ESP_LOGE(TAG, "failed to allocate motor struct");
        return ESP_ERR_NO_MEM;
    }

    mtr->in1_pin = config->in1_pin;
    mtr->in2_pin = config->in2_pin;
    mtr->en_pin = config->en_pin;

    mtr->ledc_channel = config->ledc_channel;
    mtr->ledc_timer = config->ledc_timer;
    mtr->ledc_mode = config->ledc_mode;

    mtr->speed = 0;

    // Rotary encoder
    mtr->encoder_a_pin = config->encoder_a_pin;
    mtr->encoder_b_pin = config->encoder_b_pin;
    mtr->encoder_pulses_per_rev = config->encoder_pulses_per_rev;
    mtr->encoder_count = 0;

    // Configure GPIOs for direction pins
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << mtr->in1_pin) | (1ULL << mtr->in2_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    if (gpio_config(&io_conf) != ESP_OK) {
        ESP_LOGE(TAG, "failed to set gpio config");
        free(mtr);
        return ESP_ERR_INVALID_STATE;
    }

        // Configure rotary encoder pins as inputs
        gpio_config_t enc_conf = {
            .pin_bit_mask = (1ULL << mtr->encoder_a_pin) | (1ULL << mtr->encoder_b_pin),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_POSEDGE,
        };
        gpio_config(&enc_conf);

        // Install ISR for encoder A pin
        gpio_install_isr_service(0);
        gpio_isr_handler_add(mtr->encoder_a_pin, l298n_motor_encoder_isr, (void *)mtr);

    // Configure LEDC timer
    ledc_timer_config_t ledc_timer = {
        .speed_mode = mtr->ledc_mode,
        .timer_num = mtr->ledc_timer,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz = config->pwm_freq_hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    if (ledc_timer_config(&ledc_timer) != ESP_OK) {
        ESP_LOGE(TAG, "failed to set ledc timer config");
        free(mtr);
        return ESP_ERR_INVALID_STATE;
    }

    // Configure LEDC channel for EN pin PWM
    ledc_channel_config_t ledc_channel = {
        .gpio_num = mtr->en_pin,
        .speed_mode = mtr->ledc_mode,
        .channel = mtr->ledc_channel,
        .timer_sel = mtr->ledc_timer,
        .duty = 0,
        .hpoint = 0,
    };
    if (ledc_channel_config(&ledc_channel) != ESP_OK) {
        ESP_LOGE(TAG, "failed to set ledc channel config");
        free(mtr);
        return ESP_ERR_INVALID_STATE;
    }

    mtr->pwm_max_duty = (1 << LEDC_TIMER_13_BIT) - 1;

    // Init motor stopped
    ESP_RETURN_ON_ERROR(gpio_set_level(mtr->in1_pin, 0), TAG, "failed to set gpio %d", mtr->in1_pin);
    ESP_RETURN_ON_ERROR(gpio_set_level(mtr->in2_pin, 0), TAG, "failed to set gpio %d", mtr->in2_pin);
    ledc_set_duty(mtr->ledc_mode, mtr->ledc_channel, 0);
    ledc_update_duty(mtr->ledc_mode, mtr->ledc_channel);

    *motor = (l298n_motor_handle_t)mtr;

    return ESP_OK;
}

    // Rotary encoder ISR
    static void IRAM_ATTR l298n_motor_encoder_isr(void *arg) {
        l298n_motor_t *mtr = (l298n_motor_t *)arg;
        int a = gpio_get_level(mtr->encoder_a_pin);
        int b = gpio_get_level(mtr->encoder_b_pin);
        if (a == b) {
            mtr->encoder_count++;
        } else {
            mtr->encoder_count--;
        }
        // Do NOT update current_angle here!
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

// Get current angle in degrees
float l298n_motor_get_angle(l298n_motor_handle_t motor) {
    l298n_motor_t *mtr = (l298n_motor_t *)motor;
    // Calculate angle from encoder_count
    return 360.0f * ((float)mtr->encoder_count / (float)mtr->encoder_pulses_per_rev);
}

// Reset encoder count and angle
esp_err_t l298n_motor_reset_angle(l298n_motor_handle_t motor) {
    l298n_motor_t *mtr = (l298n_motor_t *)motor;
    mtr->encoder_count = 0;
    // current_angle is now calculated on demand
    return ESP_OK;
}

// Drive to target angle (blocking, simple implementation)
esp_err_t l298n_motor_drive_to_angle(l298n_motor_handle_t motor, float target_angle, int8_t speed_percent) {
    float current = l298n_motor_get_angle(motor);
    int8_t dir = (target_angle > current) ? speed_percent : -speed_percent;
    l298n_motor_set_speed(motor, dir);
    while (fabsf(l298n_motor_get_angle(motor) - target_angle) > 1.0f) {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    l298n_motor_stop(motor);
    return ESP_OK;
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
    config->encoder_a_pin = mtr->encoder_a_pin;
    config->encoder_b_pin = mtr->encoder_b_pin;
    config->encoder_pulses_per_rev = mtr->encoder_pulses_per_rev;

    return config;
}
