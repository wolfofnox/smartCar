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

void set_handlers();

esp_err_t not_found_handler(httpd_req_t* req, httpd_err_code_t error);
esp_err_t status_handler(httpd_req_t* req);
esp_err_t control_handler(httpd_req_t* req);
esp_err_t restart_handler(httpd_req_t* req);
esp_err_t data_json_handler(httpd_req_t* req);
esp_err_t websocket_handler(httpd_req_t *req);

#endif // WIFI_STA_HANDLERS_H