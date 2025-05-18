#include "Wifi.h"

/**
 * @file Wifi.c
 * @brief WiFi and captive portal management for ESP32.
 * 
 * This file implements WiFi initialization, captive portal, event handlers,
 * and web server endpoints for configuration and control.
 */

#pragma region Variables

// Event group for WiFi state management
static EventGroupHandle_t wifi_event_group;

// Event bits for various WiFi states and actions
static const int CONNECTED_BIT = BIT0;
static const int SWITCH_TO_STA_BIT = BIT1;
static const int SWITCH_TO_CAPTIVE_AP_BIT = BIT2;
static const int RECONECT_BIT = BIT3;
static const int mDNS_CHANGE_BIT = BIT4;

// HTTP server handle and configuration
httpd_handle_t server = NULL;
static int sta_fails_count = 0;

static httpd_config_t httpd_config = HTTPD_DEFAULT_CONFIG();
captive_portal_config captive_cfg = { 0 };
esp_netif_t *ap_netif, *sta_netif;


// HTML page binary symbols (linked at build time, defined in CMakeLists.txt)
extern const char captive_portal_html_start[] asm("_binary_captive_portal_html_start");
extern const char captive_portal_html_end[] asm("_binary_captive_portal_html_end");

#pragma endregion

#pragma region Main

/**
 * @brief Initialize WiFi and start the mode switch task.
 * 
 * Sets up event groups, network interfaces, HTTP server config,
 * and launches the WiFi mode switch FreeRTOS task.
 */
void wifi_init() {
    esp_log_level_set(__FILE__, LOG_LEVEL_SOURCE);
    ESP_LOGI(__FILE__, "Initializing WiFi...");
    
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Register event handlers for WiFi and IP events
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    // Configure HTTP server
    httpd_config.lru_purge_enable = true;
    httpd_config.max_uri_handlers = 16;
    
    // Set up default HTTP server configuration
    ap_netif = esp_netif_create_default_wifi_ap();
    sta_netif = esp_netif_create_default_wifi_sta();
    
    // Init WiFi and set default configurations
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Initialize captive config
    fill_captive_portal_config_struct(&captive_cfg);
    strcpy(captive_cfg.ap_ssid, "ESP32-Captive-Portal");

    // Read NVS settings
    get_nvs_wifi_settings(&captive_cfg);
    ESP_LOGI(__FILE__, "STA SSID: %s, password: %s", captive_cfg.ssid, captive_cfg.password);
    if (captive_cfg.ssid[0] == 0) {
        ESP_LOGI(__FILE__, "No STA SSID not configured, launching captive portal AP mode...");
        xEventGroupSetBits(wifi_event_group, SWITCH_TO_CAPTIVE_AP_BIT);
    } else {
        ESP_LOGI(__FILE__, "STA SSID configured, switching to STA mode...");
        xEventGroupSetBits(wifi_event_group, SWITCH_TO_STA_BIT);
    }
    ESP_LOGI(__FILE__, "AP SSID: %s, password: %s", captive_cfg.ap_ssid, captive_cfg.ap_password);

    // Start WiFi mode switch task
    xTaskCreate(wifi_event_group_listener_task, "wifi_event_group_listener_task", 4096, NULL, 4, NULL);
}

void get_nvs_wifi_settings(captive_portal_config *cfg) {
    ESP_LOGD(__FILE__, "Reading NVS WiFi settings...");
    if (cfg == NULL) {
        ESP_LOGE(__FILE__, "Invalid configuration pointer (== NULL)");
    }
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_WIFI, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        size_t len = sizeof(cfg->ssid);
        nvs_get_str(nvs_handle, "ssid", cfg->ssid, &len);
        len = sizeof(cfg->password);
        nvs_get_str(nvs_handle, "password", cfg->password, &len);
        len = sizeof(cfg->ap_ssid);
        nvs_get_str(nvs_handle, "ap_ssid", cfg->ap_ssid, &len);
        len = sizeof(cfg->ap_password);
        nvs_get_str(nvs_handle, "ap_password", cfg->ap_password, &len);
        nvs_get_u8(nvs_handle, "use_static_ip", (uint8_t*)&cfg->use_static_ip);
        nvs_get_u8(nvs_handle, "use_mDNS", (uint8_t*)&cfg->use_mDNS);
        nvs_get_u32(nvs_handle, "static_ip", &cfg->static_ip.addr);
        len = sizeof(cfg->mDNS_hostname);
        nvs_get_str(nvs_handle, "mDNS_hostname", cfg->mDNS_hostname, &len);
        len = sizeof(cfg->service_name);
        nvs_get_str(nvs_handle, "service_name", cfg->service_name, &len);
        nvs_close(nvs_handle);
    } else {
        ESP_LOGW(__FILE__, "Failed to open NVS namespace: %s", esp_err_to_name(err));
    }
}

void set_nvs_wifi_settings(captive_portal_config *cfg) {
    ESP_LOGD(__FILE__, "Writing NVS WiFi settings...");
    int8_t n = 0;
    nvs_handle_t nvs_handle;
    captive_portal_config saved_cfg = {0};
    fill_captive_portal_config_struct(&saved_cfg);
    get_nvs_wifi_settings(&saved_cfg);
    esp_err_t err = nvs_open(NVS_NAMESPACE_WIFI, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        if (strcmp(cfg->ssid, saved_cfg.ssid) != 0) {
            nvs_set_str(nvs_handle, "ssid", cfg->ssid);
            n++;
        }
        if (strcmp(cfg->password, saved_cfg.password) != 0) {
            nvs_set_str(nvs_handle, "password", cfg->password);
            n++;
        }
        if (strcmp(cfg->ap_ssid, saved_cfg.ap_ssid) != 0) {
            nvs_set_str(nvs_handle, "ap_ssid", cfg->ap_ssid);
            n++;
        }
        if (strcmp(cfg->ap_password, saved_cfg.ap_password) != 0) {
            nvs_set_str(nvs_handle, "ap_password", cfg->ap_password);
            n++;
        }
        if (cfg->use_static_ip != saved_cfg.use_static_ip) {
            nvs_set_u8(nvs_handle, "use_static_ip", (uint8_t)cfg->use_static_ip);
            n++;
        }
        if (cfg->use_mDNS != saved_cfg.use_mDNS) {
            nvs_set_u8(nvs_handle, "use_mDNS", (uint8_t)cfg->use_mDNS);
            n++;
        }
        if (cfg->static_ip.addr != saved_cfg.static_ip.addr) {
            nvs_set_u32(nvs_handle, "static_ip", cfg->static_ip.addr);
            n++;
        }
        if (strcmp(cfg->mDNS_hostname, saved_cfg.mDNS_hostname) != 0) {
            nvs_set_str(nvs_handle, "mDNS_hostname", cfg->mDNS_hostname);
            n++;
        }
        if (strcmp(cfg->service_name, saved_cfg.service_name) != 0) {
            nvs_set_str(nvs_handle, "service_name", cfg->service_name);
            n++;
        }
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGD(__FILE__, "NVS WiFi settings written, %d changes made", n);
    } else {
        ESP_LOGW(__FILE__, "Failed to open NVS namespace: %s", esp_err_to_name(err));
    }
}

/**
 * @brief Create WiFi configuration for station (client) mode.
 * 
 * This function fills the WiFi configuration structure with the
 * provided captive portal settings.
 * 
 * @param cfg Pointer to the captive portal configuration structure.
 * 
 * @return WiFi configuration structure for station mode.
 */
wifi_config_t sta_wifi_config(captive_portal_config *cfg) {
    wifi_config_t wifi_cfg;
    esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg);

    strcpy((char *)wifi_cfg.sta.ssid, cfg->ssid);
    strcpy((char *)wifi_cfg.sta.password, cfg->password);

    wifi_cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;

    ESP_LOGD(__FILE__, "STA config set: SSID: %s, password: %s", wifi_cfg.sta.ssid, wifi_cfg.sta.password);

    return wifi_cfg;
}
    
/**
 * @brief Create WiFi configuration for captive portal AP mode.
 * 
 * This function fills the WiFi configuration structure with the
 * provided captive portal settings.
 * 
 * @param cfg Pointer to the captive portal configuration structure.
 * 
 * @return WiFi configuration structure for AP mode.
 */
wifi_config_t ap_wifi_config(captive_portal_config *cfg) {
    wifi_config_t wifi_cfg;
    esp_wifi_get_config(WIFI_IF_AP, &wifi_cfg);

    strcpy((char *)wifi_cfg.ap.ssid, cfg->ap_ssid);
    strcpy((char *)wifi_cfg.ap.password, cfg->ap_password);
    wifi_cfg.ap.ssid_len = strlen(cfg->ap_ssid);
    wifi_cfg.ap.max_connection = 4;

    if (cfg->ap_password[0] == 0) {
        wifi_cfg.ap.authmode = WIFI_AUTH_OPEN;
    } else {
        wifi_cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    }

    ESP_LOGD(__FILE__, "AP config set: SSID: %s, password: %s, authmode: %d", wifi_cfg.ap.ssid, wifi_cfg.ap.password, wifi_cfg.ap.authmode);
    

    return wifi_cfg;
}

/**
 * @brief Fill the captive portal configuration structure with empty values.
 * 
 * This function initializes the captive portal configuration structure
 * with empty data to ensure it is ready for use.
 * 
 * @param cfg Pointer to the captive portal configuration structure to fill.
 */
void fill_captive_portal_config_struct(captive_portal_config *cfg) {
    strcpy(cfg->ssid, "");
    strcpy(cfg->password, "");
    cfg->use_static_ip = false;
    cfg->static_ip.addr = 0;
    cfg->use_mDNS = false;
    strcpy(cfg->mDNS_hostname, "");
    strcpy(cfg->service_name, "");
    strcpy(cfg->ap_ssid, "");
    strcpy(cfg->ap_password, "");
}

/**
 * @brief FreeRTOS task to handle WiFi mode switching and related events.
 * 
 * Waits for event bits to be set and performs actions such as switching
 * between STA/AP modes, reconnecting, or updating mDNS.
 * 
 * @param pvParameter Unused.
 */
void wifi_event_group_listener_task(void *pvParameter) {
    while (1) {
        ESP_LOGD(__FILE__, "Waiting for event bits...");
        // Wait for any relevant event bit
        EventBits_t eventBits = xEventGroupWaitBits(
            wifi_event_group,
            SWITCH_TO_STA_BIT | SWITCH_TO_CAPTIVE_AP_BIT | RECONECT_BIT | mDNS_CHANGE_BIT,
            pdFALSE, pdFALSE, portMAX_DELAY);
        ESP_LOGD(__FILE__, "Recieved event bits: %s%s%s%s%s%s%s%s%s%s",
            eventBits & BIT9 ? "1" : "0",
            eventBits & BIT8 ? "1" : "0",
            eventBits & BIT7 ? "1" : "0",
            eventBits & BIT6 ? "1" : "0",
            eventBits & BIT5 ? "1" : "0",
            eventBits & BIT4 ? "1" : "0",
            eventBits & BIT3 ? "1" : "0",
            eventBits & BIT2 ? "1" : "0",
            eventBits & BIT1 ? "1" : "0",
            eventBits & BIT0 ? "1" : "0");
        vTaskDelay(100 / portTICK_PERIOD_MS);

        wifi_mode_t mode;
        if (esp_wifi_get_mode(&mode) == ESP_ERR_WIFI_NOT_INIT) {
            mode = WIFI_MODE_NULL;
        }

        // Switch to STA mode
        if (eventBits & SWITCH_TO_STA_BIT) {
            ESP_LOGI(__FILE__, "Switching to STA mode...");
            led_indicator_start(led_handle, BLINK_WIFI_CONNECTING);
            if (server) {
                httpd_stop(server);
                server = NULL;
            }
            if (eventBits & CONNECTED_BIT) {
                ESP_LOGW(__FILE__, "Already connected to AP, no need to switch.");
                xEventGroupClearBits(wifi_event_group, SWITCH_TO_STA_BIT);
                continue;
            }
            esp_wifi_stop();
            mdns_free(); // Free mDNS if exists
            xEventGroupClearBits(wifi_event_group, SWITCH_TO_STA_BIT);
            wifi_init_sta();
        }

        // Switch to captive AP mode
        if (eventBits & SWITCH_TO_CAPTIVE_AP_BIT) {
            ESP_LOGI(__FILE__, "Switching to AP captive portal mode...");
            led_indicator_start(led_handle, BLINK_WIFI_AP_STARTING);
            if (server) {
                httpd_stop(server);
                server = NULL;
            }
            esp_wifi_disconnect();
            esp_wifi_stop();
            mdns_free(); // Free mDNS if exists
            wifi_init_captive();
            xEventGroupClearBits(wifi_event_group, SWITCH_TO_CAPTIVE_AP_BIT);
        }

        // Reconnect in STA mode
        if (eventBits & RECONECT_BIT && mode == WIFI_MODE_STA) {
            ESP_LOGD(__FILE__, "Reconnecting to AP...");
            esp_wifi_disconnect();
            ESP_LOGD(__FILE__, "Waiting for disconnect...");
            while (xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT) {
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
            led_indicator_start(led_handle, BLINK_WIFI_CONNECTING);

            xEventGroupClearBits(wifi_event_group, RECONECT_BIT);

            wifi_config_t wifi_cfg = sta_wifi_config(&captive_cfg);
            esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);

            // Set static or dynamic IP
            esp_netif_dhcpc_stop(sta_netif);
            if (captive_cfg.use_static_ip) {
                uint32_t new_ip = ntohl(captive_cfg.static_ip.addr);
                esp_netif_ip_info_t ip_info;
                ip_info.ip.addr = captive_cfg.static_ip.addr;
                ip_info.gw.addr = htonl((new_ip & 0xFFFFFF00)|0x01);    // x.x.x.1
                ip_info.netmask.addr = htonl((255 << 24) | (255 << 16) | (255 << 8) | 0);   // 255.255.255.0
                esp_netif_set_ip_info(sta_netif, &ip_info);
            } else {
                esp_netif_ip_info_t ip_info = {0};
                esp_netif_set_ip_info(sta_netif, &ip_info);
                esp_netif_dhcpc_start(sta_netif);
            }
            esp_wifi_connect();
        }

        // Update mDNS settings
        if (eventBits & mDNS_CHANGE_BIT && mode == WIFI_MODE_STA) {
            if (captive_cfg.use_mDNS) {
                mdns_init(); // Initialize mDNS if not already done
                ESP_ERROR_CHECK(mdns_hostname_set(captive_cfg.mDNS_hostname));
                ESP_ERROR_CHECK(mdns_instance_name_set(captive_cfg.service_name));
                ESP_LOGI(__FILE__, "mDNS hostname updated: %s", captive_cfg.mDNS_hostname);
                ESP_LOGI(__FILE__, "mDNS service name updated: %s", captive_cfg.service_name);
                mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0); // Add mDNS service if not already done
            } else {
                mdns_free(); // Free mDNS if exists
                ESP_LOGI(__FILE__, "mDNS removed");
            }
            xEventGroupClearBits(wifi_event_group, mDNS_CHANGE_BIT);
        }
    }
} 

/**
 * @brief Initialize WiFi in captive portal AP mode.
 * 
 * Sets up the ESP32 as a WiFi access point, starts the HTTP server,
 * registers captive portal handlers, and starts a DNS server for redirection.
 */
void wifi_init_captive() {
    ESP_LOGI(__FILE__, "Starting AP mode for captive portal...");

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    wifi_config_t wifi_cfg = ap_wifi_config(&captive_cfg);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_cfg));
    wifi_cfg = sta_wifi_config(&captive_cfg);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Log AP IP address
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(ap_netif, &ip_info);
    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    ESP_LOGI(__FILE__, "Set up softAP with IP: %s", ip_addr);

    if (captive_cfg.ap_password[0] != 0) {
        ESP_LOGI(__FILE__, "SoftAP started: SSID:' %s' Password: '%s'", captive_cfg.ap_ssid, captive_cfg.ap_password);
    } else {
        ESP_LOGI(__FILE__, "SoftAP started: SSID:' %s' No password", captive_cfg.ap_ssid);
    }

    // Start HTTP server and register handlers
    ESP_LOGV(__FILE__, "Starting web server on port: %d", httpd_config.server_port);
    ESP_ERROR_CHECK(httpd_start(&server, &httpd_config));

    httpd_uri_t captive_portal = {
        .uri = "/captive_portal",
        .method = HTTP_GET,
        .handler = captive_portal_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &captive_portal));
    httpd_uri_t captive_portal_post = {
        .uri = "/captive_portal",
        .method = HTTP_POST,
        .handler = captive_portal_post_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &captive_portal_post));
    httpd_uri_t captive_json = {
        .uri = "/captive.json",
        .method = HTTP_GET,
        .handler = captive_json_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &captive_json));
    httpd_uri_t scan_json = {
        .uri = "/scan.json",
        .method = HTTP_GET,
        .handler = scan_json_handler
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &scan_json));

    ESP_ERROR_CHECK(httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, captive_redirect));

    // Start DNS server for captive portal redirection (highjack all DNS queries)
    dns_server_config_t dns_config = DNS_SERVER_CONFIG_SINGLE("*" /* all A queries */, "WIFI_AP_DEF" /* softAP netif ID */);
    start_dns_server(&dns_config);
}

/**
 * @brief Initialize WiFi in station (client) mode.
 * 
 * Connects to the configured WiFi network, starts the HTTP server,
 * registers handlers, and optionally starts mDNS.
 */
void wifi_init_sta() {
    ESP_LOGI(__FILE__, "Starting WiFi in station mode...");
    
    wifi_config_t wifi_cfg = sta_wifi_config(&captive_cfg);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Set static IP if requested
    
    esp_netif_ip_info_t ip_info = {0};
    esp_netif_dhcpc_stop(sta_netif);
    if (captive_cfg.use_static_ip) {
        uint32_t new_ip = ntohl(captive_cfg.static_ip.addr);
        ip_info.ip.addr = captive_cfg.static_ip.addr;
        ip_info.gw.addr = htonl((new_ip & 0xFFFFFF00)|0x01);    // x.x.x.1
        ip_info.netmask.addr = htonl((255 << 24) | (255 << 16) | (255 << 8) | 0);   // 255.255.255.0
        esp_netif_set_ip_info(sta_netif, &ip_info);
    } else {
        esp_netif_set_ip_info(sta_netif, &ip_info);
        esp_netif_dhcpc_start(sta_netif);
    }
    
    // Log IP address
    esp_netif_get_ip_info(sta_netif, &ip_info);
    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    ESP_LOGD(__FILE__, "Set up STA with IP: %s", ip_addr);

    ESP_LOGD(__FILE__, "Starting web server on port: %d", httpd_config.server_port);
    ESP_ERROR_CHECK(httpd_start(&server, &httpd_config));

    // Register captive portal HTTP handlers (on /captive_portal for STA mode)
    httpd_uri_t captive_portal_uri = {
        .uri = "/captive_portal",
        .method = HTTP_GET,
        .handler = captive_portal_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &captive_portal_uri);

    httpd_uri_t captive_portal_post_uri = {
        .uri = "/captive_portal",
        .method = HTTP_POST,
        .handler = captive_portal_post_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &captive_portal_post_uri);

    httpd_uri_t captive_json_uri = {
        .uri = "/captive.json",
        .method = HTTP_GET,
        .handler = captive_json_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &captive_json_uri);

    httpd_uri_t scan_json_uri = {
        .uri = "/scan.json",
        .method = HTTP_GET,
        .handler = scan_json_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &scan_json_uri);

    // Register STA handlers
    set_handlers();

    // Start mDNS if enabled
    if (captive_cfg.use_mDNS) {
        ESP_ERROR_CHECK(mdns_init());
        ESP_ERROR_CHECK(mdns_hostname_set(captive_cfg.mDNS_hostname));
        ESP_ERROR_CHECK(mdns_instance_name_set(captive_cfg.service_name));
        ESP_LOGI(__FILE__, "mDNS started: http://%s.local", captive_cfg.mDNS_hostname);
        ESP_LOGI(__FILE__, "mDNS service started: %s", captive_cfg.service_name);
        mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    }
}

#pragma endregion

#pragma region Captive Portal Handlers

/**
 * @brief HTTP handler for serving the captive portal HTML page.
 */
esp_err_t captive_portal_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    const uint32_t captive_portal_html_len = captive_portal_html_end - captive_portal_html_start;
    httpd_resp_send(req, (const char *)captive_portal_html_start, captive_portal_html_len);
    ESP_LOGD(__FILE__, "Captive portal page served");
    return ESP_OK;
}

/**
 * @brief HTTP error handler for redirecting to the captive portal.
 */
esp_err_t captive_redirect(httpd_req_t *req, httpd_err_code_t error) {
    httpd_resp_set_status(req, "302 Temporary Redirect");
    httpd_resp_set_hdr(req, "Location", "/captive_portal");
    httpd_resp_send(req, "Redirected to captive portal", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief HTTP handler for scanning available WiFi networks and returning JSON results.
 */
esp_err_t scan_json_handler(httpd_req_t *req) {
    char json[256];
    uint16_t ap_count = 0;
    wifi_scan_config_t scan_config = {
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 0,
        .scan_time.active.max = 0
    };
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    wifi_ap_record_t ap_records[ap_count];
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records));

    snprintf(json, sizeof(json), "{\"ap_count\": %d, \"aps\": [", ap_count);
    for (int i = 0; i < ap_count; i++) {
        char ssid[33];
        snprintf(ssid, sizeof(ssid), "%s", ap_records[i].ssid);
        if (i < ap_count - 1) {
            snprintf(json + strlen(json), sizeof(json) - strlen(json), "{\"ssid\": \"%s\", \"rssi\": %d},", ssid, ap_records[i].rssi);
        } else {
            snprintf(json + strlen(json), sizeof(json) - strlen(json), "{\"ssid\": \"%s\", \"rssi\": %d}", ssid, ap_records[i].rssi);
        }
        snprintf(json + strlen(json), sizeof(json) - strlen(json), "]}");
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    ESP_LOGD(__FILE__, "Scan results sent: %s", json);
    return ESP_OK;
}

/**
 * @brief HTTP handler for returning saved captive portal configuration as JSON.
 */
esp_err_t captive_json_handler(httpd_req_t *req) {
    char json[512]; // max 330 bytes
    snprintf(json, sizeof(json),
        "{\"ssid\": \"%s\", \"password\": \"%s\", \"use_static_ip\": %s, \"static_ip\": \"%s\", \"use_mDNS\": %s, \"mDNS_hostname\": \"%s\", \"service_name\": \"%s\"}",
        captive_cfg.ssid,
        captive_cfg.password,
        captive_cfg.use_static_ip ? "true" : "false",
        inet_ntoa(captive_cfg.static_ip.addr),
        captive_cfg.use_mDNS ? "true" : "false",
        captive_cfg.mDNS_hostname,
        captive_cfg.service_name
    );
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    ESP_LOGD(__FILE__, "Captive portal JSON data sent: %s", json);
    return ESP_OK;
}

/**
 * @brief HTTP POST handler for updating captive portal configuration.
 * 
 * Parses POST data, updates config, and triggers reconnect or mDNS update as needed.
 */
esp_err_t captive_portal_post_handler(httpd_req_t *req) {
    char buf[256];
    int len = httpd_req_recv(req, buf, MIN(req->content_len, sizeof(buf) - 1));
    bool need_reconnect = false;
    bool need_mdns_update = false;
    wifi_mode_t mode;
    ESP_ERROR_CHECK(esp_wifi_get_mode(&mode));
    if (len > 0) {
        buf[len] = '\0';
        char param[32];
        if (httpd_query_key_value(buf, "ssid", param, sizeof(param)) == ESP_OK) {
            if (strcmp((char*)&captive_cfg.ssid, param) != 0) {
                if (mode == WIFI_MODE_STA) {
                    need_reconnect = true;
                    ESP_LOGD(__FILE__, "SSID changed, reconnecting...");
                }
                strcpy((char*)&captive_cfg.ssid, param);
            }
        }
        if (httpd_query_key_value(buf, "password", param, sizeof(param)) == ESP_OK) {
            if (strlen(param) != 0 && strcmp((char*)&captive_cfg.password, param) != 0) {
                if (mode == WIFI_MODE_STA) {
                    need_reconnect = true;
                    ESP_LOGD(__FILE__, "Password changed, reconnecting...");
                }
                strcpy((char*)&captive_cfg.password, param);
            }
        }
        if (httpd_query_key_value(buf, "use_static_ip", param, sizeof(param)) == ESP_OK) {
            bool new_use_static_ip = strcmp(param, "true") == 0;
            if (captive_cfg.use_static_ip != new_use_static_ip) {
                if (mode == WIFI_MODE_STA) {
                    need_reconnect = true;
                    ESP_LOGD(__FILE__, "Static IP usage changed, reconnecting...");
                }
            }
            captive_cfg.use_static_ip = new_use_static_ip;
        } else {
            if (captive_cfg.use_static_ip) {
                if (mode == WIFI_MODE_STA) {
                    need_reconnect = true;
                    ESP_LOGD(__FILE__, "Static IP usage disabled, reconnecting...");
                }
            }
            captive_cfg.use_static_ip = false;
        }
        if (httpd_query_key_value(buf, "static_ip", param, sizeof(param)) == ESP_OK) {
            uint32_t new_ip = inet_addr(param);
            if (captive_cfg.static_ip.addr != new_ip && captive_cfg.use_static_ip) {
                if (mode == WIFI_MODE_STA) {
                    need_reconnect = true;
                    ESP_LOGD(__FILE__, "Static IP changed, reconnecting...");
                }
            }
            captive_cfg.static_ip.addr = new_ip;
        }
        if (httpd_query_key_value(buf, "use_mDNS", param, sizeof(param)) == ESP_OK) {
            bool new_use_mdns = strcmp(param, "true") == 0;
            if (captive_cfg.use_mDNS != new_use_mdns) {
                if (mode == WIFI_MODE_STA) {
                    need_mdns_update = true;
                    ESP_LOGD(__FILE__, "mDNS usage changed, updating...");
                }
            }
            captive_cfg.use_mDNS = new_use_mdns;
        } else {
            if (captive_cfg.use_mDNS) {
                if (mode == WIFI_MODE_STA) {
                    need_mdns_update = true;
                    ESP_LOGD(__FILE__, "mDNS usage disabled, updating...");
                }
            }
            captive_cfg.use_mDNS = false;
        }
        if (httpd_query_key_value(buf, "mDNS_hostname", param, sizeof(param)) == ESP_OK) {
            if ((strcmp(captive_cfg.mDNS_hostname, param) != 0)) {
                if (captive_cfg.use_mDNS) {
                    if (mode == WIFI_MODE_STA) {
                        need_mdns_update = true;
                        ESP_LOGD(__FILE__, "mDNS hostname changed, updating...");
                    }
                }
                strcpy(captive_cfg.mDNS_hostname, param);
            }
        }
        if (httpd_query_key_value(buf, "service_name", param, sizeof(param)) == ESP_OK) {
            // Replace '+' with ' ' in param
            for (char *p = param; *p; ++p) {
                if (*p == '+') *p = ' ';
            }
            if ((strcmp(captive_cfg.service_name, param) != 0)) {
                if (captive_cfg.use_mDNS) {
                    if (mode == WIFI_MODE_STA) {
                        need_mdns_update = true;
                        ESP_LOGD(__FILE__, "mDNS service name changed, updating...");
                    }
                }
                strcpy(captive_cfg.service_name, param);
            }
        }
    }

    // Log the updated captive portal settings
    ESP_LOGD(__FILE__, "Captive portal settings saved");
    ESP_LOGV(__FILE__, "SSID: %s", captive_cfg.ssid);
    ESP_LOGV(__FILE__, "Password: %s", captive_cfg.password);
    ESP_LOGV(__FILE__, "Use static IP: %s", captive_cfg.use_static_ip ? "true" : "false");
    char ip_str[16];
    inet_ntoa_r(captive_cfg.static_ip.addr, ip_str, 16);
    ESP_LOGV(__FILE__, "Static IP: %s", ip_str);
    ESP_LOGV(__FILE__, "Use mDNS: %s", captive_cfg.use_mDNS ? "true" : "false");
    ESP_LOGV(__FILE__, "mDNS hostname: %s", captive_cfg.mDNS_hostname);
    ESP_LOGV(__FILE__, "Service name: %s", captive_cfg.service_name);

    // Save settings to NVS
    set_nvs_wifi_settings(&captive_cfg);

    if (mode == WIFI_MODE_STA) {
        // Check if it is needed to reconnect to the AP or update and restart mDNS
        if (need_reconnect) {
            xEventGroupSetBits(wifi_event_group, RECONECT_BIT);
        }
        if (need_mdns_update) {
            xEventGroupSetBits(wifi_event_group, mDNS_CHANGE_BIT);
        }
        
        // Redirect back to captive portal, method GET
        httpd_resp_set_status(req, "302 Temporary Redirect");
        httpd_resp_set_hdr(req, "Location", "/captive_portal");
        httpd_resp_send(req, "Redirected", HTTPD_RESP_USE_STRLEN);
        ESP_LOGV(__FILE__, "Redirecting to back captive portal, method GET");
        return ESP_OK;
    } else {
        xEventGroupSetBits(wifi_event_group, SWITCH_TO_STA_BIT);
        return ESP_OK;
    }
}

#pragma endregion

#pragma region Wifi Event Handler

/**
 * @brief WiFi and IP event handler.
 * 
 * Handles AP/STA connect/disconnect, IP acquisition, and triggers mode switches.
 */
void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    EventBits_t bits = xEventGroupGetBits(wifi_event_group);
    wifi_mode_t mode;
    ESP_ERROR_CHECK(esp_wifi_get_mode(&mode));
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(__FILE__, "Wi-Fi AP started."); 
        led_indicator_stop(led_handle, BLINK_WIFI_AP_STARTING);
        led_indicator_start(led_handle, BLINK_WIFI_AP_STARTED);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGD(__FILE__, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGD(__FILE__, "station " MACSTR " leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START && mode == WIFI_MODE_STA) {
        ESP_LOGI(__FILE__, "Wi-Fi STA started, connecting...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;
        ESP_LOGI(__FILE__, "Connected to AP: %s", event->ssid);
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        sta_fails_count = 0;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        led_indicator_stop(led_handle, BLINK_WIFI_CONNECTING);
        led_indicator_stop(led_handle, BLINK_WIFI_CONNECTED);
        led_indicator_start(led_handle, BLINK_WIFI_DISCONNECTED);
        if ((bits & RECONECT_BIT) == 0 && mode == WIFI_MODE_STA && (bits & SWITCH_TO_CAPTIVE_AP_BIT) == 0) {
            ESP_LOGW(__FILE__, "Wi-Fi disconnected, reconnecting...");
            sta_fails_count++;
            if (sta_fails_count >= MAX_STA_FAILS) {
                ESP_LOGW(__FILE__, "Max STA reconect fails reached, switching to AP mode...");
                esp_wifi_disconnect();
                sta_fails_count = 0;
                xEventGroupSetBits(wifi_event_group, SWITCH_TO_CAPTIVE_AP_BIT);
                return;
            } else {
                ESP_LOGD(__FILE__, "Reconnecting...");
                esp_wifi_connect();
                led_indicator_start(led_handle, BLINK_WIFI_CONNECTING);
            }
        } else {
            ESP_LOGD(__FILE__, "Wi-Fi disconnected.");
        } 
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        char ip_str[IP4ADDR_STRLEN_MAX];
        esp_ip4addr_ntoa(&event->ip_info.ip, ip_str, IP4ADDR_STRLEN_MAX);
        ESP_LOGI(__FILE__, "Got IP: %s", ip_str);
        sta_fails_count = 0;
        led_indicator_stop(led_handle, BLINK_WIFI_CONNECTING);
        led_indicator_start(led_handle, BLINK_WIFI_CONNECTED);
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    } else {
        ESP_LOGW(__FILE__, "Unhandled event: %s:%ld", event_base, event_id);
    }
}

#pragma endregion
