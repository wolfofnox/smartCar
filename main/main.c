/*
 * ESP-IDF Project template
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#include "Wifi.h"
#include "wifi_sta_handlers.h"

#include "servo.h"
#include "l298n_motor.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_sleep.h"
#include "ssd1306.h"
// #include "font8x8_basic.h"
#include "nvs_flash.h"
#include "nvs.h"


// --- Define variables, classes ---
#pragma region Variables

int64_t bootTime;   ///< System boot time in microseconds
const char *TAG = "main";  ///< Log tag for this module
const char *NVS_NAMESPACE_APP = "app_settings";
#define VOLTAGE_DIVIDER_RATIO ((CONFIG_VOLTAGE_DIVIDER_R1 + CONFIG_VOLTAGE_DIVIDER_R2) / CONFIG_VOLTAGE_DIVIDER_R1)

typedef enum {
    BATTERY_WALL_ADAPTER, ///< Wall adapter power supply
    BATTERY_6xNiMH        ///< 6x NiMH rechargeable batteries
} battery_type_t;

adc_cali_handle_t adc_cali;
adc_oneshot_unit_handle_t adc_unit;

SSD1306_t display;

servo_handle_t steeringServo = NULL; ///< Handle for the steering servo
servo_handle_t topServo = NULL; ///< Handle for the throttle servo
l298n_motor_handle_t motor = NULL;

// nvs setting variables
servo_config_t steeringCfg = {
    .gpio_num = CONFIG_PIN_STEERING_SERVO,
    .min_pulsewidth_us = 1270,
    .max_pulsewidth_us = 2080,
    .min_degree = -90,
    .max_degree = 90,
    .period_ticks = 20000,
    .resolution_hz = 1000000,
};
servo_config_t topCfg = {
    .gpio_num = CONFIG_PIN_TOP_SERVO,
    .min_pulsewidth_us = 500,
    .max_pulsewidth_us = 2400,
    .min_degree = -90,
    .max_degree = 90,
    .period_ticks = 20000,
    .resolution_hz = 1000000,
};
l298n_motor_config_t motorCfg = {
    .en_pin = CONFIG_PIN_MOT_EN,
    .in1_pin = CONFIG_PIN_MOT_F,
    .in2_pin = CONFIG_PIN_MOT_R,
    .ledc_channel = LEDC_CHANNEL_0,
    .ledc_mode = LEDC_LOW_SPEED_MODE,
    .ledc_timer = LEDC_TIMER_0,
    .pwm_freq_hz = 5000
};
battery_type_t batteryType = BATTERY_6xNiMH; ///< Type of battery used in the car

#pragma endregion

// --- Define functions ---
#pragma region Functions

float get_battery_voltage();
void check_battery();
void deep_sleep();
void check_battery_task(void *pvParameter);
void load_nvs_calibration();
void save_nvs_calibration();
void load_nvs_settings();
void save_nvs_settings();

#pragma endregion

#pragma region App main
void app_main(void)
{
    // Set log level
    esp_log_level_set("*", CONFIG_LOG_LEVEL_GLOBAL);
    esp_log_level_set(TAG, CONFIG_LOG_LEVEL_SOURCE);
    esp_log_level_set("Web socket", CONFIG_LOG_LEVEL_SOURCE);
    ESP_LOGI(TAG, "START %s from %s", __FILE__, __DATE__);
    ESP_LOGI(TAG, "Setting up...");

    // Configure GPIO 3V3 bus output
    gpio_config_t v_bus_config = {
        .pin_bit_mask = (1ULL << CONFIG_PIN_3V3_BUS),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&v_bus_config));
    ESP_ERROR_CHECK(gpio_set_level(CONFIG_PIN_3V3_BUS, 1));

    // SSD1306 display config
    i2c_master_init(&display, CONFIG_PIN_I2C_SDA, CONFIG_PIN_I2C_SCL, -1);
    display._flip = true;
    ssd1306_init(&display, 128, 64);
    ssd1306_clear_screen(&display, false);
    ssd1306_contrast(&display, 0xff);

    // init ADC
    adc_oneshot_unit_init_cfg_t adc_unit_cfg = {
        .unit_id = CONFIG_ADC_UNIT_BATTERY_VOLTAGE,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE
    };
    adc_oneshot_new_unit(&adc_unit_cfg, &adc_unit);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12
    };
    adc_oneshot_config_channel(adc_unit, CONFIG_ADC_CHANNEL_BATTERY_VOLTAGE, &chan_cfg);

    adc_cali_curve_fitting_config_t adc_cali_cfg = {
        .unit_id = CONFIG_ADC_UNIT_BATTERY_VOLTAGE,
        .chan = CONFIG_ADC_CHANNEL_BATTERY_VOLTAGE,
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12
    };
    adc_cali_create_scheme_curve_fitting(&adc_cali_cfg, &adc_cali);

    xTaskCreate(check_battery_task, "check_battery_task", 4096, NULL, 6, NULL);

    ESP_LOGI(__FILE__, "Initializing NVS...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    load_nvs_calibration(); // Load NVS configuration
    
    // Servo config
    ESP_ERROR_CHECK(servo_init(&steeringServo, &steeringCfg));
    ESP_ERROR_CHECK(servo_init(&topServo, &topCfg));

    // DC motor config
    ESP_ERROR_CHECK(l298n_motor_init(&motor, &motorCfg));

    wifi_init();

    set_handlers();

    // Get boot time for uptime calculation
    bootTime = esp_timer_get_time();
}

float get_battery_voltage() {
    int voltage_mv;
    adc_oneshot_get_calibrated_result(adc_unit, adc_cali, CONFIG_ADC_CHANNEL_BATTERY_VOLTAGE, &voltage_mv);
    return voltage_mv / 1000.0 * VOLTAGE_DIVIDER_RATIO;
}

void check_battery() {
    const char *TAG = "check_battery";
    float voltage = get_battery_voltage();
    char buff[32];
    snprintf(buff, sizeof(buff), "%2.2fV", voltage);
    ssd1306_clear_line(&display, 2, false);
    ssd1306_clear_line(&display, 3, false);
    ssd1306_display_text(&display, 1, "Voltage:", 8, false);
    ssd1306_display_text(&display, 2, buff, strlen(buff), false);
    switch (batteryType) {
        case BATTERY_WALL_ADAPTER:
            ssd1306_display_text(&display, 5, "Wall adapter", 12, false);
            break;
        case BATTERY_6xNiMH:
            ssd1306_display_text(&display, 5, "6x NiMH battery", 15, false);
            if (voltage < 1) {
                voltage = 0;
                ssd1306_display_text(&display, 3, "Connect battery", 15, false);
                return;
            }
            if (voltage < 6.0) {
                ESP_LOGE(TAG, "Battery voltage critical: %fV", voltage);
                ESP_LOGW(TAG, "Please charge the batteries!");
                ESP_LOGW(TAG, "Shutting down...");
                deep_sleep();
                return;
            }
            if (voltage < 7.0) {
                ssd1306_display_text(&display, 3, "Battery low!", 13, true);
            }
        default:
            break;
    }
}

void deep_sleep() {
    servo_deinit(steeringServo);
    servo_deinit(topServo);
    l298n_motor_deinit(motor);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    gpio_set_level(CONFIG_PIN_3V3_BUS, 0);
    esp_deep_sleep_start();
}

void check_battery_task(void *pvParameter) {
    while (1) {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        check_battery();
    }
}

/**
 * @brief Load configuration from NVS (Non-Volatile Storage).
 * 
 * Loads servo and motor configuration to the global variables.
 * If NVS is not initialized or keys are not found, default values are used.
 */
void load_nvs_calibration() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_APP, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        size_t len = sizeof(steeringCfg);
        nvs_get_blob(nvs_handle, "steering_cfg", &steeringCfg, &len);
        len = sizeof(topCfg);
        nvs_get_blob(nvs_handle, "top_cfg", &topCfg, &len);
        len = sizeof(motorCfg);
        nvs_get_blob(nvs_handle, "motor_cfg", &motorCfg, &len);
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE(__FILE__, "Failed to open NVS for saving config: %s", esp_err_to_name(err));
    }
}

void save_nvs_calibration() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_APP, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_blob(nvs_handle, "steering_cfg", &steeringCfg, sizeof(steeringCfg));
        nvs_set_blob(nvs_handle, "top_cfg", &topCfg, sizeof(topCfg));
        nvs_set_blob(nvs_handle, "motor_cfg", &motorCfg, sizeof(motorCfg));
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(__FILE__, "NVS calibration saved successfully");
    } else {
        ESP_LOGE(__FILE__, "Failed to open NVS for saving config: %s", esp_err_to_name(err));
    }
}

void load_nvs_settings() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_APP, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_get_u8(nvs_handle, "battery_type", (uint8_t *)&batteryType);
        nvs_close(nvs_handle);
    } else {
        ESP_LOGE(__FILE__, "Failed to open NVS for loading settings: %s", esp_err_to_name(err));
    }
}

void save_nvs_settings() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_APP, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_u8(nvs_handle, "battery_type", (uint8_t)batteryType);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(__FILE__, "NVS settings saved successfully");
    } else {
        ESP_LOGE(__FILE__, "Failed to open NVS for saving settings: %s", esp_err_to_name(err));
    }
}