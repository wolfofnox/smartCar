#include "wifi_sta_handlers.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "Wifi.h"
#include "l298n_motor.h"
#include "servo.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

typedef struct {
    int16_t min_value;
    int16_t max_value;
    bool min_value_set;
    bool max_value_set;
} ws_pulsewidth_limits_buffer_t;

extern int64_t bootTime;  ///< System boot time in microseconds

extern httpd_handle_t server;

extern servo_handle_t steeringServo;
extern servo_handle_t topServo;
extern l298n_motor_handle_t motor;

extern servo_config_t steeringCfg; ///< Steering servo configuration
extern servo_config_t topCfg; ///< Top servo configuration
extern l298n_motor_config_t motorCfg; ///< Motor configuration

extern void save_nvs_calibration(); ///< Save configuration to NVS

static TimerHandle_t ws_watchdog_timer = NULL; ///< WebSocket timeout watchdog timer handle

static uint32_t ws_watchdog_timeout = 5000; ///< WebSocket timeout in milliseconds
static int ws_socket_fd = -1; ///< WebSocket socket file descriptor

static ws_pulsewidth_limits_buffer_t ws_steering_limits_buffer = {0};
static ws_pulsewidth_limits_buffer_t ws_top_limits_buffer = {0};

#define TAG "WiFi Handlers"
#define TAG_WS "WebSocket"

esp_err_t calibrate_post_handler(httpd_req_t *req);

esp_err_t status_json_handler(httpd_req_t *req);

esp_err_t websocket_handler(httpd_req_t *req);
void ws_watchdog_callback(TimerHandle_t xTimer);
void ws_watchdog_start();

/**
 * @brief Register HTTP URI handlers for the web server in station mode.
 * 
 * Registers all endpoints for captive portal, control, status, and data.
 */
void set_handlers() {
    ESP_LOGI(TAG, "Setting up uri handlers...");

    httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = websocket_handler,
        .is_websocket = true,
        .handle_ws_control_frames = true
    };
    wifi_register_http_handler(&ws_uri);

    httpd_uri_t calibrate_post_uri = {
        .uri = "/calibrate",
        .method = HTTP_POST,
        .handler = calibrate_post_handler
    };
    wifi_register_http_handler(&calibrate_post_uri);

    httpd_uri_t status_json_uri = {
        .uri = "/status.json",
        .method = HTTP_GET,
        .handler = status_json_handler
    };
    wifi_register_http_handler(&status_json_uri);
}


/**
 * @brief HTTP handler for returning JSON data about the ESP32 status.
 */
esp_err_t status_json_handler(httpd_req_t *req) {
    char json[300];
    int free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    int total_heap = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    snprintf(json, sizeof(json), "{\"uptime\": %lli, \"freeHeap\": %d, \"totalHeap\": %d, \"version\": \"%s\", \"speed\": %d, \"steering\": %d, \"top\": %d, \"steeringMinPWM\": %li, \"steeringMaxPWM\": %li, \"steeringMinAngle\": %d, \"steeringMaxAngle\": %d, \"topMinPWM\": %li, \"topMaxPWM\": %li, \"topMinAngle\": %d, \"topMaxAngle\": %d}",
             (esp_timer_get_time() - bootTime) / 1000, free_heap, total_heap, CONFIG_VERSION,
            servo_get_angle(steeringServo), servo_get_angle(topServo), l298n_motor_get_speed(motor),
            steeringCfg.min_pulsewidth_us, steeringCfg.max_pulsewidth_us, steeringCfg.min_degree, steeringCfg.max_degree,
            topCfg.min_pulsewidth_us, topCfg.max_pulsewidth_us, topCfg.min_degree, topCfg.max_degree);
    ESP_LOGD(TAG, "JSON data requested: %s", json);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, strlen(json));
}


esp_err_t calibrate_post_handler(httpd_req_t *req) {
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        return ESP_FAIL; // Error or no data received
    }
    buf[len] = '\0'; // Null-terminate the string

    // Parse the POST data
    char param[32];
    if (httpd_query_key_value(buf, "steering_pulsewidth_limits", param, sizeof(param)) == ESP_OK) {
        int min_pwm = 0, max_pwm = 0;
        if(sscanf(param + 1, "%d,%d", &min_pwm, &max_pwm) == 2) {
            steeringCfg.min_pulsewidth_us = min_pwm;
            steeringCfg.max_pulsewidth_us = max_pwm;
            servo_set_nim_max_pulsewidth(steeringServo, min_pwm, max_pwm);
        }
    }
    if (httpd_query_key_value(buf, "steering_angle_limits", param, sizeof(param)) == ESP_OK) {
        int min_angle = 0, max_angle = 0;
        if (sscanf(param + 1, "%d,%d", &min_angle, &max_angle) == 2) {
            steeringCfg.min_degree = min_angle;
            steeringCfg.max_degree = max_angle;
            servo_set_nim_max_degree(steeringServo, min_angle, max_angle);
        }
    }
    if (httpd_query_key_value(buf, "steering_center_position", param, sizeof(param)) == ESP_OK) {
        // not yet implemented
    }

    ESP_LOGI(TAG, "Calibration POST data received: %s", buf);
    ESP_LOGI(TAG, "Steering pulsewidth limits: %lu - %lu", steeringCfg.min_pulsewidth_us, steeringCfg.max_pulsewidth_us);
    ESP_LOGI(TAG, "Steering angle limits: %d - %d", steeringCfg.min_degree, steeringCfg.max_degree);


    save_nvs_calibration(); // Save the updated configuration to NVS

    httpd_resp_set_status(req, "302 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/calibrate");  
    httpd_resp_send(req, "Calibration successful", HTTPD_RESP_USE_STRLEN);
    ESP_LOGV(TAG, "Redirecting to calibration page after POST");
    return ESP_OK;
}

esp_err_t websocket_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        // Initial handshake, just return OK
        ws_socket_fd = httpd_req_to_sockfd(req);
        ws_watchdog_start(); // Start the watchdog timer
        ESP_LOGI(TAG_WS, "WebSocket connection established");
        return ESP_OK;
    }

    // Handle incoming WebSocket frame
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = NULL;

    ESP_RETURN_ON_ERROR(httpd_ws_recv_frame(req, &ws_pkt, 0), TAG_WS, "Failed to receive WebSocket frame");

    ws_watchdog_start(); // Start the watchdog timer

    if (ws_pkt.type == HTTPD_WS_TYPE_BINARY && ws_pkt.len == 1) {
        ws_pkt.payload = malloc(1);
        esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 1);
        if (ret != ESP_OK) {
            free(ws_pkt.payload);
            ESP_RETURN_ON_ERROR(ret, TAG_WS, "Failed to receive ws event packet");
        }
        uint8_t event_id = ((uint8_t*)ws_pkt.payload)[0];
        switch (event_id) {
            case EVENT_TIMEOUT:
                ws_watchdog_callback(NULL); // Reset power save mode
                ESP_LOGV(TAG_WS, "WebSocket timeout event received");
                break;
            case EVENT_ESTOP:
                servo_set_angle(steeringServo, 0);
                servo_set_angle(topServo, 0);
                l298n_motor_set_speed(motor, 0);
                ESP_LOGV("Web socket", "Emergency stop activated");
                break;
            case EVENT_REVERT_SETTINGS:
                ESP_LOGV(TAG_WS, "Reverting to default settings");
                servo_set_nim_max_pulsewidth(steeringServo, steeringCfg.min_pulsewidth_us, steeringCfg.max_pulsewidth_us);
                servo_set_nim_max_pulsewidth(topServo, topCfg.min_pulsewidth_us, topCfg.max_pulsewidth_us);
                l298n_motor_set_speed(motor, 0); // Stop the motor
                break;
            default:
                ESP_LOGW("Web socket", "Unknown event id: 0x%2X", event_id);
        }
        free(ws_pkt.payload);
        return ESP_OK;
    }

    // Check if this is a binary control packet with a numerical value
    if (ws_pkt.type == HTTPD_WS_TYPE_BINARY && ws_pkt.len == sizeof(ws_control_packet_t)) {
        ws_pkt.payload = malloc(ws_pkt.len);
        esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            free(ws_pkt.payload);
            ESP_RETURN_ON_ERROR(ret, TAG_WS, "Failed to receive ws control packet");
        }
        ws_control_packet_t *packet = (ws_control_packet_t *)ws_pkt.payload;
        switch(packet->type) {
            case CONTROL_SPEED:
                l298n_motor_set_speed(motor, packet->value);
                ESP_LOGV(TAG_WS, "Set motor speed to %d", packet->value);
                break;
            case CONTROL_STEERING:
                servo_set_angle(steeringServo, packet->value);
                ESP_LOGV(TAG_WS, "Set steering angle to %d", packet->value);
                break;
            case CONTROL_TOP_SERVO:
                servo_set_angle(topServo, packet->value);
                ESP_LOGV(TAG_WS, "Set top servo angle to %d", packet->value);
                break;
            case CONFIG_STEERING_MIN_PULSEWIDTH:
                ws_steering_limits_buffer.min_value = packet->value;
                ws_steering_limits_buffer.min_value_set = true;
                if (ws_steering_limits_buffer.max_value_set) {
                    servo_set_nim_max_pulsewidth(steeringServo, ws_steering_limits_buffer.min_value, ws_steering_limits_buffer.max_value);
                    ESP_LOGV(TAG_WS, "Set steering limits to [%u, %u]", ws_steering_limits_buffer.min_value, ws_steering_limits_buffer.max_value);
                }
                break;
            case CONFIG_STEERING_MAX_PULSEWIDTH:
                ws_steering_limits_buffer.max_value = packet->value;
                ws_steering_limits_buffer.max_value_set = true;
                if (ws_steering_limits_buffer.min_value_set) {
                    servo_set_nim_max_pulsewidth(steeringServo, ws_steering_limits_buffer.min_value, ws_steering_limits_buffer.max_value);
                    ESP_LOGV(TAG_WS, "Set steering limits to [%u, %u]", ws_steering_limits_buffer.min_value, ws_steering_limits_buffer.max_value);
                }
                break;
            case CONFIG_TOP_MIN_PULSEWIDTH:
                ws_top_limits_buffer.min_value = packet->value;
                ws_top_limits_buffer.min_value_set = true;
                if (ws_top_limits_buffer.max_value_set) {
                    servo_set_nim_max_pulsewidth(topServo, ws_top_limits_buffer.min_value, ws_top_limits_buffer.max_value);
                    ESP_LOGV(TAG_WS, "Set top limits to [%u, %u]", ws_top_limits_buffer.min_value, ws_top_limits_buffer.max_value);
                }
                break;
            case CONFIG_TOP_MAX_PULSEWIDTH:
                ws_top_limits_buffer.max_value = packet->value;
                ws_top_limits_buffer.max_value_set = true;
                if (ws_top_limits_buffer.min_value_set) {
                    servo_set_nim_max_pulsewidth(topServo, ws_top_limits_buffer.min_value, ws_top_limits_buffer.max_value);
                    ESP_LOGV(TAG_WS, "Set top limits to [%u, %u]", ws_top_limits_buffer.min_value, ws_top_limits_buffer.max_value);
                }
                break;
            case CONFIG_WS_TIMEOUT:
                if (packet->value > 0) {
                    ws_watchdog_timeout = packet->value;
                    ESP_LOGV(TAG_WS, "Set WebSocket timeout to %lu ms", ws_watchdog_timeout);
                    ws_watchdog_start(); // Restart the watchdog with the new timeout
                } else {
                    ESP_LOGW(TAG_WS, "Invalid ws timeout value recieved: %d", packet->value);
                }
                break;
            default:
                ESP_LOGW(TAG_WS, "Unknown control type: 0x%2X, value: 0x%4X", packet->type, packet->value);
        }
        free(ws_pkt.payload);
        return ESP_OK;
    }

    // Handle WS packets
    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        ESP_LOGI(TAG_WS, "WebSocket connection closed");
        ws_watchdog_callback(NULL); // Reset power save mode
        free(ws_pkt.payload);
        return ESP_OK;
    }

    // Fallback for other messages, null terminate payload
    ws_pkt.payload = calloc(1, ws_pkt.len + 1);
    ws_pkt.payload[ws_pkt.len] = 0;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
        free(ws_pkt.payload);
        ESP_RETURN_ON_ERROR(ret, TAG_WS, "Failed to receive WebSocket frame payload");
    }

    ESP_LOGV(TAG_WS, "Received text payload: %s", (char *)ws_pkt.payload);

    free(ws_pkt.payload);
    return ESP_OK;
}

void ws_watchdog_callback(TimerHandle_t xTimer) {
    ESP_LOGD(TAG_WS, "WebSocket timed out, resetting power save mode");
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM); // Re-enable power save mode
    
    // Restore servo config from global variables
    servo_set_nim_max_pulsewidth(steeringServo, steeringCfg.min_pulsewidth_us, steeringCfg.max_pulsewidth_us);
    servo_set_nim_max_degree(steeringServo, steeringCfg.min_degree, steeringCfg.max_degree);
    servo_set_nim_max_pulsewidth(topServo, topCfg.min_pulsewidth_us, topCfg.max_pulsewidth_us);
    servo_set_nim_max_degree(topServo, topCfg.min_degree, topCfg.max_degree);

    if (ws_socket_fd != -1) {
        uint8_t payload = (uint8_t)EVENT_TIMEOUT;
        httpd_ws_frame_t ws_frame = {
            .type = HTTPD_WS_TYPE_BINARY,
            .payload = &payload,
            .len = sizeof(payload)
        };
        httpd_ws_send_frame_async(server, ws_socket_fd, &ws_frame);
    }
}

void ws_watchdog_start() {
    if (ws_watchdog_timer != NULL) {
        xTimerStop(ws_watchdog_timer, 0);
        xTimerChangePeriod(ws_watchdog_timer, pdMS_TO_TICKS(ws_watchdog_timeout), 0);
    }
    if (ws_watchdog_timer == NULL) {
        ws_watchdog_timer = xTimerCreate("ws_watchdog", pdMS_TO_TICKS(ws_watchdog_timeout), pdFALSE, NULL, ws_watchdog_callback);
    }
    xTimerStart(ws_watchdog_timer, 0);    
    wifi_ps_type_t ps_type;
    esp_wifi_get_ps(&ps_type);
    if (ps_type != WIFI_PS_NONE) {
        esp_wifi_set_ps(WIFI_PS_NONE); // Disable power save mode for WebSocket
    }
}
