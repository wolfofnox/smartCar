#include "wifi_sta_handlers.h"

/**
 * @brief Register HTTP URI handlers for the web server in station mode.
 * 
 * Registers all endpoints for captive portal, control, status, and data.
 */
void set_handlers() {
    ESP_LOGI(__FILE__, "Setting up uri handlers...");

    httpd_uri_t status_uri = {
        .uri = "/status",
        .method = HTTP_GET,
        .handler = status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &status_uri);

    httpd_uri_t control_uri = {
        .uri = "/control",
        .method = HTTP_GET,
        .handler = control_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &control_uri);

    httpd_uri_t data_json_uri = {
        .uri = "/data.json",
        .method = HTTP_GET,
        .handler = data_json_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &data_json_uri);

    httpd_uri_t restart_uri = {
        .uri = "/restart",
        .method = HTTP_GET,
        .handler = restart_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &restart_uri);

    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, not_found_handler);
}

/**
 * @brief HTTP error handler for 404.
 */
esp_err_t not_found_handler(httpd_req_t *req, httpd_err_code_t error) {
    char text[256];
    size_t len = 0;
    len += snprintf(text + len, sizeof(text) - len, "404 Not Found\n\n");
    len += snprintf(text + len, sizeof(text) - len, "URI: %s\n", req->uri);
    len += snprintf(text + len, sizeof(text) - len, "Method: %s\n", (req->method == HTTP_GET) ? "GET" : "POST");
    len += snprintf(text + len, sizeof(text) - len, "Arguments:\n");
    char query[128];
    size_t query_len = httpd_req_get_url_query_len(req) + 1;
    if (query_len > 1) {
        httpd_req_get_url_query_str(req, query, query_len);
        len += snprintf(text + len, sizeof(text) - len, "%s\n", query);
    }
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, text, HTTPD_RESP_USE_STRLEN);
    ESP_LOGW(__FILE__, "%s", text);
    return ESP_FAIL;
}

/**
 * @brief HTTP handler for /status endpoint (returns HTML status page).
 */
esp_err_t status_handler(httpd_req_t *req) {
    char html[256];
    char ip_str[IP4ADDR_STRLEN_MAX];
    esp_netif_ip_info_t ip_info;
    if (sta_netif && esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
        esp_ip4addr_ntoa(&ip_info.ip, ip_str, IP4ADDR_STRLEN_MAX);
    } else {
        strcpy(ip_str, "N/A");
    }
    uint32_t time_s = (esp_timer_get_time() - bootTime) / 1000000;
    uint32_t time_m = time_s / 60;
    time_s %= 60;
    uint16_t time_h = time_m / 60;
    time_m %= 60;
    uint8_t time_d = time_h / 24;
    time_h %= 24;
    snprintf(html, sizeof(html), "<!DOCTYPE html>"
                                  "<html>"
                                  "<head><title>ESP32 Status</title></head>"
                                  "<body>"
                                  "<h1>ESP32 Status</h1>"
                                  "<p>Uptime: %hud, %uh, %lum, %lus</p>"
                                  "<p>LED State: %s</p>"
                                  "<p>LED Color: #%06lx</p>"
                                  "<p>Local IP: %s</p>"
                                  "</body>"
                                  "</html>",
             time_d, time_h, time_m, time_s, (ledOn ? "On" : "Off"), led_indicator_get_rgb(led_handle), ip_str);
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief HTTP handler for /control endpoint (returns HTML control page).
 */
esp_err_t control_handler(httpd_req_t *req) {
    char query[128];
    size_t query_len = httpd_req_get_url_query_len(req) + 1;
    if (query_len > 1) {
        httpd_req_get_url_query_str(req, query, query_len);
        char param[32];
        if (httpd_query_key_value(query, "led", param, sizeof(param)) == ESP_OK) {
            if (strcmp(param, "on") == 0) {
                ESP_LOGV(__FILE__, "Set LED state: on");
                ledOn = true;
                led_indicator_set_on_off(led_handle, true);
            } else {
                ESP_LOGV(__FILE__, "Set LED state: off");
                ledOn = false;
                led_indicator_set_on_off(led_handle, false);
            }
        }
        if (httpd_query_key_value(query, "color", param, sizeof(param)) == ESP_OK) {
            ESP_LOGD(__FILE__, "Set color value: %s", param);
            ESP_LOGV(__FILE__, "Set color value: 0x%06lx", strtol(param + 1, NULL, 16));
            led_indicator_set_rgb(led_handle, strtol(param + 1, NULL, 16));
        }
        httpd_resp_set_status(req, "302 Temporary Redirect");
        httpd_resp_set_hdr(req, "Location", "/control");
        httpd_resp_send(req, "Control updated", HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_send(req, "<!DOCTYPE html>"
                         "<html>"
                         "<head><title>ESP32 Control Page</title></head>"
                         "<body>"
                         "<h1>Control Page</h1>"
                         "<p>Control your ESP32 here:</p>"
                         "<ul>"
                         "<li><a href=\"/control?led=on\">Turn LED On</a></li>"
                         "<li><a href=\"/control?led=off\">Turn LED Off</a></li>"
                         "<li>Set LED Color: <a href=\"/control?color=xff0000\">Red</a>, "
                         "<a href=\"/control?color=x00ff00\">Green</a>, "
                         "<a href=\"/control?color=x0000ff\">Blue</a></li>"
                         "</ul>"
                         "</body>"
                         "</html>", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief HTTP handler for /restart endpoint (restarts ESP32).
 */
esp_err_t restart_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, "Restarting...", HTTPD_RESP_USE_STRLEN);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    mdns_free();
    esp_restart();
    return ESP_OK;
}

/**
 * @brief HTTP handler for returning JSON data about the ESP32 status.
 */
esp_err_t data_json_handler(httpd_req_t *req) {
    char json[128];
    char ip_str[IP4ADDR_STRLEN_MAX];
    esp_netif_ip_info_t ip_info;
    if (sta_netif && esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
        esp_ip4addr_ntoa(&ip_info.ip, ip_str, IP4ADDR_STRLEN_MAX);
    } else {
        strcpy(ip_str, "N/A");
    }
    snprintf(json, sizeof(json), "{\"ledState\": \"%s\", \"uptime\": %lli, \"ledColor\": \"#%06lx\", \"localIP\": \"%s\"}",  
             (ledOn ? "on" : "off"), (esp_timer_get_time() - bootTime) / 1000, led_indicator_get_rgb(led_handle), ip_str);
    ESP_LOGD(__FILE__, "JSON data requested: %s", json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    return ESP_OK;
}
