#ifndef WIFI_HANDLER_H
#define WIFI_HANDLER_H

#include "Config.h"
#include "wifi_sta_handlers.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_http_server.h"
#include "esp_mac.h"      // for MAC2STR macro
#include "dns_server.h"   // for captive portal DNS hijack
#include "lwip/inet.h"
#include <sys/param.h>    // for MIN macro
#include "mdns.h"

// Extern variables for system status
extern int64_t bootTime;  ///< System boot time in microseconds
extern bool ledOn;        ///< LED state (on/off)

extern led_indicator_handle_t led_handle; ///< Handle for the LED indicator
extern httpd_handle_t server; ///< HTTP server handle
extern esp_netif_t *ap_netif, *sta_netif; ///< Network interface handles for AP and STA modes

/**
 * @brief Configuration structure for captive portal and WiFi settings.
 */
typedef struct {
    char ssid[32];              ///< SSID of the WiFi network
    char password[64];          ///< Password for the WiFi network
    bool use_static_ip;         ///< Use static IP if true, DHCP otherwise
    esp_ip4_addr_t static_ip;   ///< Static IP address (if enabled)
    bool use_mDNS;              ///< Enable mDNS if true
    char mDNS_hostname[32];     ///< mDNS hostname (e.g., "esp32")
    char service_name[64];      ///< mDNS service name (e.g., "ESP32 Web")

    char ap_ssid[32];           ///< SSID of the captive portal AP
    char ap_password[64];       ///< Password for the captive portal AP
} captive_portal_config;

extern captive_portal_config captive_cfg;

void wifi_init();
void wifi_init_captive();
void wifi_init_sta();

void get_nvs_wifi_settings(captive_portal_config *cfg);
void set_nvs_wifi_settings(captive_portal_config *cfg);
void fill_captive_portal_config_struct(captive_portal_config *cfg);

wifi_config_t ap_wifi_config(captive_portal_config *cfg);
wifi_config_t sta_wifi_config(captive_portal_config *cfg);

void wifi_event_group_listener_task(void *pvParameter);

esp_err_t captive_redirect(httpd_req_t* req, httpd_err_code_t error);
esp_err_t captive_portal_handler(httpd_req_t* req);
esp_err_t captive_portal_post_handler(httpd_req_t* req);
esp_err_t captive_json_handler(httpd_req_t* req);
esp_err_t scan_json_handler(httpd_req_t* req);

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

#endif