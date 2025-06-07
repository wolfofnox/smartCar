/*
 * ESP-IDF Project template
 */

#include "Config.h"

// --- Define variables, classes ---
#pragma region Variables

int64_t bootTime;   ///< System boot time in microseconds
bool ledOn = false; ///< LED state (on/off)

adc_cali_handle_t adc_cali;
adc_oneshot_unit_handle_t adc_unit;

SSD1306_t display;

led_indicator_handle_t led_handle; ///< Handle for the LED indicator
const blink_step_t *led_blink_list[BLINK_MAX] = {
    [BLINK_OFF] = off,
    [BLINK_LOADING] = loading,
    [BLINK_LOADED] = loaded,
    [BLINK_WIFI_CONNECTING] = wifi_connecting,
    [BLINK_WIFI_CONNECTED] = wifi_connected,
    [BLINK_WIFI_DISCONNECTED] = wifi_disconnected,
    [BLINK_WIFI_AP_STARTING] = wifi_ap_starting,
    [BLINK_WIFI_AP_STARTED] = wifi_ap_started
};

servo_handle_t steeringServo = NULL; ///< Handle for the steering servo
servo_handle_t topServo = NULL; ///< Handle for the throttle servo
l298n_motor_handle_t motor = NULL;

#pragma endregion

// --- Define functions ---
#pragma region Functions

float get_battery_voltage();
void check_battery();
void deep_sleep();
void check_battery_task(void *pvParameter);

#pragma endregion

#pragma region App main
void app_main(void)
{
    // Set log level
    esp_log_level_set("*", LOG_LEVEL_GLOBAL);
    esp_log_level_set(__FILE__, LOG_LEVEL_SOURCE);
    esp_log_level_set("Web socket", ESP_LOG_DEBUG);
    ESP_LOGI(__FILE__, "START %s from %s", __FILE__, __DATE__);
    ESP_LOGI(__FILE__, "Setting up...");
    
    // Configure GPIO 3V3 bus output
    gpio_config_t v_bus_config = {
        .pin_bit_mask = (1ULL << PIN_3V3_BUS),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&v_bus_config));
    ESP_ERROR_CHECK(gpio_set_level(PIN_3V3_BUS, 1));

    // Configure LED indicator
    led_indicator_strips_config_t led_indicator_strips_cfg = {
        .led_strip_cfg = {
            .strip_gpio_num = PIN_LED,  ///< GPIO number for the LED strip
            .max_leds = 1,               ///< Maximum number of LEDs in the strip
            .led_pixel_format = LED_PIXEL_FORMAT_GRB,  ///< Pixel format
            .led_model = LED_MODEL_SK6812,  ///< LED driver model
            .flags.invert_out = 0,       ///< Invert output signal
        },
        .led_strip_driver = LED_STRIP_SPI,
        .led_strip_spi_cfg = {
            .clk_src = SPI_CLK_SRC_DEFAULT,  ///< SPI clock source
            .spi_bus = SPI3_HOST,  ///< SPI bus host
        },
    };
    led_indicator_config_t led_cfg = {
        .mode = LED_STRIPS_MODE,  ///< LED mode (e.g., LED_STRIP_MODE)
        .led_indicator_strips_config = &led_indicator_strips_cfg,
        .blink_lists = led_blink_list,
        .blink_list_num = BLINK_MAX, 
    };

    led_handle = led_indicator_create(&led_cfg);
    if (led_handle == NULL) {
        ESP_LOGE(__FILE__, "Failed to create LED indicator");
    }

    led_indicator_start(led_handle, BLINK_LOADING); // Start LED indicator with loading animation

    // SSD1306 display config
    i2c_master_init(&display, PIN_I2C_SDA, PIN_I2C_SCL, -1);
    display._flip = true;
    ssd1306_init(&display, 128, 64);
    ssd1306_clear_screen(&display, false);
    ssd1306_contrast(&display, 0xff);

    // Servo config
    gpio_config_t motor_config = {
        .pin_bit_mask = (1ULL << PIN_MOT_1) | (1ULL << PIN_MOT_2),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&motor_config));
    ESP_ERROR_CHECK(gpio_set_level(PIN_MOT_1, 0));
    ESP_ERROR_CHECK(gpio_set_level(PIN_MOT_2, 0));

    servo_config_t steeringCfg = {
        .gpio_num = PIN_STEER_SERVO,
        .min_pulsewidth_us = 1200,
        .max_pulsewidth_us = 1700,
        .min_degree = -90,
        .max_degree = 90,
        .period_ticks = 20000,
        .resolution_hz = 1000000,
    };
    steeringServo = servo_init(&steeringCfg);
    servo_config_t topCfg = {
        .gpio_num = PIN_TOP_SERVO,
        .min_pulsewidth_us = 500,
        .max_pulsewidth_us = 2400,
        .min_degree = -90,
        .max_degree = 90,
        .period_ticks = 20000,
        .resolution_hz = 1000000,
    };
    topServo = servo_init(&topCfg);

    // DC motor config
    l298n_motor_config_t motorCfg = {
        .en_pin = PIN_MOT_EN,
        .in1_pin = PIN_MOT_1,
        .in2_pin = PIN_MOT_2,
        .ledc_channel = LEDC_CHANNEL_0,
        .ledc_mode = LEDC_LOW_SPEED_MODE,
        .ledc_timer = LEDC_TIMER_0,
        .pwm_freq_hz = 5000
    };
    motor = l298n_motor_init(&motorCfg);

    // init ADC
    adc_oneshot_unit_init_cfg_t adc_unit_cfg = {
        .unit_id = ADC_UNIT_BAT_VOLT,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE
    };
    adc_oneshot_new_unit(&adc_unit_cfg, &adc_unit);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12
    };
    adc_oneshot_config_channel(adc_unit, ADC_CHANNEL_BAT_VOLT, &chan_cfg);

    adc_cali_curve_fitting_config_t adc_cali_cfg = {
        .unit_id = ADC_UNIT_BAT_VOLT,
        .chan = ADC_CHANNEL_BAT_VOLT,
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12
    };
    adc_cali_create_scheme_curve_fitting(&adc_cali_cfg, &adc_cali);

    xTaskCreate(check_battery_task, "check_battery_task", 4096, NULL, 6, NULL);

    #if USE_NVS == 1 // Initialize NVS (Non-Volatile Storage)
    ESP_LOGI(__FILE__, "Initializing NVS...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    #endif

    #if USE_WiFi == 1 // Initialize WiFi
    wifi_init();
    #endif

    // Get boot time for uptime calculation
    bootTime = esp_timer_get_time();
    led_indicator_stop(led_handle, BLINK_LOADING);
    led_indicator_start(led_handle, BLINK_LOADED);

    int speed = 0;
    int step = 5;
    while (0) {
        ESP_LOGI(__FILE__, "Speedn: %d", speed);
        ESP_ERROR_CHECK(l298n_motor_set_speed(motor, speed));
        vTaskDelay(pdMS_TO_TICKS(100));
        if ((speed + step) > 100 || (speed + step) < -100) {
            step *= -1;
        }
        speed += step;
    }
}

float get_battery_voltage() {
    int32_t voltage_mv;
    adc_oneshot_get_calibrated_result(adc_unit, adc_cali, ADC_CHANNEL_BAT_VOLT, &voltage_mv);
    return voltage_mv / 1000.0 * BATTERY_VOLTAGE_MULTIPLIER;
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
    #if BATTERY_TYPE == BATTERY_WALL_ADAPTER

    #elif BATTERY_TYPE == BATTERY_6xNiMH
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
    #endif
}

void deep_sleep() {
    servo_deinit(steeringServo);
    servo_deinit(topServo);
    l298n_motor_deinit(motor);
    vTaskDelay(1000/portTICK_PERIOD_MS);
    gpio_set_level(PIN_3V3_BUS, 0);
    esp_deep_sleep_start();
}

void check_battery_task(void *pvParameter) {
    while (1) {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        check_battery();
    }
}