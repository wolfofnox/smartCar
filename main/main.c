/*
 * ESP-IDF Project template
 */

#include "Config.h"

// --- Define variables, classes ---
#pragma region Variables

int64_t bootTime;   ///< System boot time in microseconds
bool ledOn = false; ///< LED state (on/off)

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

#pragma endregion

// --- Define functions ---
#pragma region Functions

#pragma endregion

#pragma region App main
void app_main(void)
{
    // Set log level
    esp_log_level_set("*", LOG_LEVEL_GLOBAL);
    esp_log_level_set(__FILE__, LOG_LEVEL_SOURCE);
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
            .spi_bus = SPI1_HOST,  ///< SPI bus host
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
}
