#ifndef WIFI_STA_HANDLERS_H
#define WIFI_STA_HANDLERS_H

#include "Wifi.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_http_server.h"
#include "lwip/inet.h"

extern int64_t bootTime;  ///< System boot time in microseconds

extern httpd_handle_t server; ///< HTTP server handle
extern esp_netif_t *ap_netif, *sta_netif;

extern servo_handle_t steeringServo;
extern servo_handle_t topServo;
extern l298n_motor_handle_t motor;

extern servo_config_t steeringCfg; ///< Steering servo configuration
extern servo_config_t topCfg; ///< Top servo configuration
extern l298n_motor_config_t motorCfg; ///< Motor configuration
extern battery_type_t batteryType; ///< Battery type

extern void save_nvs_calibration(); ///< Save configuration to NVS

void set_handlers();

esp_err_t not_found_handler(httpd_req_t* req, httpd_err_code_t error);

esp_err_t root_handler(httpd_req_t* req);

esp_err_t styles_css_handler(httpd_req_t* req);
esp_err_t comon_js_handler(httpd_req_t* req);
esp_err_t ws_js_handler(httpd_req_t* req);
esp_err_t nav_handler(httpd_req_t* req);

esp_err_t status_handler(httpd_req_t* req);
esp_err_t control_handler(httpd_req_t* req);
esp_err_t restart_handler(httpd_req_t* req);
esp_err_t calibrate_handler(httpd_req_t *req);
esp_err_t calibrate_post_handler(httpd_req_t *req);

esp_err_t data_json_handler(httpd_req_t* req);

esp_err_t websocket_handler(httpd_req_t *req);
void ws_watchdog_callback(TimerHandle_t xTimer);
void ws_watchdog_start();

#endif // WIFI_STA_HANDLERS_H