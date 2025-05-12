#include <WifiHandler.h>
#if USE_WiFi == 1

AsyncWebServer Server(80); // Create a web server on port 80
AsyncWebSocket ws("/ws"); // Create a WebSocket server on "/ws"

void wifi_init() {
  sd_init(); // Initialize SD card

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  #ifdef STATIC_IP
  log_i("Connecting to: " MY_SSID " with a static IP: %s", STATIC_IP.toString().c_str());
  WiFi.config(STATIC_IP, GATEWAY, SUBNET);
  #else
  log_i("Connecting to: %s\n", MY_SSID);
  #endif

  WiFi.begin(MY_SSID, MY_PASS);
  leds[0] = CRGB(121, 255, 0);
  FastLED.show();

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }
  log_i("Connected. Local IP: %s", WiFi.localIP().toString());

  leds[0] = CRGB(50, 255, 0);
  FastLED.show();

  #if USE_mDNS == 1
  if (MDNS.begin(mDNS_HOSTNAME)) {
    log_i("mDNS responder started: http://" mDNS_HOSTNAME ".local/");
  } else {
    log_e("Error setting up mDNS responder!");
  }
  #endif

  set_callbacks();
  Server.begin();
  #if USE_mDNS == 1
  MDNS.addService(SERVICE_NAME, "tcp", 80); // Add HTTP service to mDNS
  #endif
  log_i("HTTP server started.");
}

#if USE_SD_SERVER == 1
void sd_init() {
  log_i("Initializing SPI...");
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI); // Initialize SPI bus

  log_i("Initializing SD card...");
  if(!SD.begin(SD_CS)) {
    log_e("Card Mount Failed");
    while (1) {
      delay(1000);
    }
  }
  if (SD.cardType() == CARD_NONE) {
    log_w("No SD card attached");
    log_w("Insert an SD card and restart the board.");
  }
}
#endif

void set_callbacks() {
  #if USE_SD_SERVER == 1
  Server.serveStatic("/", SD, "/");
  Server.serveStatic("/status", SD, "/status.html");
  Server.serveStatic("/control", SD, "/control.html");
  #else
  Server.on("/", HTTP_GET,  [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hello from ESP32!");
  });
  Server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<!DOCTYPE html>"
                  "<html>"
                  "<head><title>ESP32 Status</title></head>"
                  "<body>"
                  "<h1>ESP32 Status</h1>"
                  "<p>Uptime: " + String((millis() - bootTime) / 1000) + " seconds</p>"
                  "<p>LED State: " + String(ledOn ? "On" : "Off") + "</p>"
                  "<p>LED Color: #" + String(leds[0].r, HEX) + String(leds[0].g, HEX) + String(leds[0].b, HEX) + "</p>"
                  "<p>Local IP: " + WiFi.localIP().toString() + "</p>"
                  "</body>"
                  "</html>";
    request->send(200, "text/html", html);
  });
  Server.on("/control", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", 
        "<!DOCTYPE html>"
        "<html>"
        "<head><title>ESP32 Control Page</title></head>"
        "<body>"
        "<h1>Control Page</h1>"
        "<p>Control your ESP32 here:</p>"
        "<ul>"
        "<li><a href=\"/control?led=on\">Turn LED On</a></li>"
        "<li><a href=\"/control?led=off\">Turn LED Off</a></li>"
        "<li>Set LED Color: <a href=\"/control?color=%23ff0000\">Red</a>, "
        "<a href=\"/control?color=%2300ff00\">Green</a>, "
        "<a href=\"/control?color=%230000ff\">Blue</a></li>"
        "</ul>"
        "</body>"
        "</html>"
    );
  });
  #endif
  Server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/", 307);
    delay(1000);
    ESP.restart();
  });
  Server.addHandler(&ws); // Attach WebSocket handler
  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
      log_d("WebSocket client connected: %u", client->id());
    } else if (type == WS_EVT_DISCONNECT) {
      log_d("WebSocket client disconnected: %u", client->id());
    } else if (type == WS_EVT_DATA) {
      AwsFrameInfo *info = (AwsFrameInfo *)arg;
      if (info->final && info->index == 0 && info->len == len) {
        data[len] = 0; // Null-terminate the data
        log_d("WebSocket message received: %s", data);

        char *speedPtr = strstr((char *)data, "\"speed\":");
        if (speedPtr) {
            int val = atoi(speedPtr + 8); // Extract and convert the value
            drive(val); // Call the drive function
            log_d("Speed set to: %d", speed);
        }

        // Parse "steering" value
        char *steeringPtr = strstr((char *)data, "\"steering\":");
        if (steeringPtr) {
            int val = atoi(steeringPtr + 11); // Extract and convert the value
            steer(val); // Call the steer function
            log_d("Steering set to: %d", steering);
        }

        // Parse "top" value
        char *topPtr = strstr((char *)data, "\"top\":");
        if (topPtr) {
            int val = atoi(topPtr + 6); // Extract and convert the value
            topServoGo(val); // Call the top servo function
            log_d("Top Servo set to: %d", top);
        }

        // Parse "estop" value
        char *estopPtr = strstr((char *)data, "\"estop\":");
        if (estopPtr) {
            ledOn = false; // Emergency stop: turn off LED
            steer(90); // Center steering
            drive(0); // Stop the car
            topServoGo(90); // Center top servo
            log_d("Emergency stop activated. Speed: %d, Steering: %d, Top: %d", speed, steering, top);
        }
      }
    }
  });
  Server.on("/slider", HTTP_GET, [](AsyncWebServerRequest *request) {
    char html[2048]; // Adjust size as needed
    snprintf(html, sizeof(html),
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
      "<title>RC Car Control</title>"
      "<style>"
        "body { font-family: Arial, sans-serif; text-align: center; margin: 20px; }"
        "h1 { color: #333; }"
        "label { display: block; margin: 10px 0 5px; }"
        "input[type='range'] { margin: 10px 0; }"
        "#speed { writing-mode: bt-lr; transform: rotate(270deg); width: 150px; margin: 80px 0; }"
        "button { padding: 10px 20px; background-color: #f44336; color: white; border: none; border-radius: 5px; cursor: pointer; }"
        "button:hover { background-color: #d32f2f; }"
      "</style>"
    "</head>"
    "<body>"
      "<h1>RC Car Control</h1>"
      "<label for='speed'>Speed:</label>"
      "<input type='range' id='speed' min='-20' max='20' value='%d' /><br>"
      "<label for='steering'>Steering:</label>"
      "<input type='range' id='steering' min='0' max='36' value='%d' /><br>"
      "<label for='top'>Top Servo:</label>"
      "<input type='range' id='top' min='0' max='36' value='%d' /><br>"
      "<button id='estop'>Emergency Stop</button>"
      "<script>"
        "const ws = new WebSocket(`ws://%s.local/ws`);"
        "ws.onopen = () => console.log('WebSocket connected');"
        "ws.onmessage = (event) => console.log('Message received:', event.data);"
        "ws.onerror = (error) => console.error('WebSocket error:', error);"
        "ws.onclose = () => console.log('WebSocket closed');"
        "function sendSliderValue(id) {"
          "const slider = document.getElementById(id);"
          "slider.addEventListener('input', () => {"
            "const msg = JSON.stringify({ [id]: Number(slider.value*5) });"
            "ws.send(msg);"
            "console.log('Message sent:', msg);"
          "});"
        "}"
        "document.getElementById('estop').addEventListener('click', () => {"
          "const msg = JSON.stringify({ estop: true });"
          "ws.send(msg);"
          "console.log('Message sent:', msg);"
          "document.getElementById('speed').value = 0;"
          "document.getElementById('steering').value = 18;"
          "document.getElementById('top').value = 18;"
        "});"
        "sendSliderValue('speed');"
        "sendSliderValue('steering');"
        "sendSliderValue('top');"
      "</script>"
    "</body>"
    "</html>",
    speed/5, steering/5, top/5, WiFi.localIP().toString().c_str()
    );
    request->send(200, "text/html", html);
  });
  Server.on("/data.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    char json[128]; // Adjust size as needed
    snprintf(json, sizeof(json), "{\"ledState\": \"%s\", \"uptime\": %lu, \"ledColor\": \"#%02x%02x%02x\", \"localIP\": \"%s\"}",  
             (ledOn ? "on" : "off"), (millis() - bootTime) / 1000, leds[0].r, leds[0].g, leds[0].b, WiFi.localIP().toString().c_str());
    log_d("JSON data requested: %s", json);
    request->send(200, "application/json", json);
  });
  Server.on("/control", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("led", true)) {
      ledOn = request->getParam("led", true)->value() == "on";
      log_d("LED state received: %s", (ledOn ? "on" : "off"));
    }
    if (request->hasParam("color", true)) {
      const char* color = request->getParam("color", true)->value().c_str();
      log_d("LED color received: %s", color);
      byte r = strtol(color + 1, nullptr, 16) >> 16; // Extract red component
      byte g = (strtol(color + 1, nullptr, 16) >> 8) & 0xFF; // Extract green component
      byte b = strtol(color + 1, nullptr, 16) & 0xFF; // Extract blue component
      leds[0] = CRGB(r, g, b); // Set LED color
    }
    if (ledOn) {
      FastLED.setBrightness(127); // Set LED to white
    } else {
      FastLED.setBrightness(0); // Turn off LED
    }
    FastLED.show(); // Update LED
    request->redirect("/control"); // Redirect to control page
  });
  Server.onNotFound(handle404);
}

void handle404(AsyncWebServerRequest *request) {
  char buffer[256]; // Fixed-size buffer
  size_t len = 0;   // Track the length of the buffer content

  // Add "404 Not Found" message
  len += snprintf(buffer + len, sizeof(buffer) - len, "404 Not Found\n\n");

  // Add URI
  len += snprintf(buffer + len, sizeof(buffer) - len, "URI: %s\n", request->url().c_str());

  // Add Method
  len += snprintf(buffer + len, sizeof(buffer) - len, "Method: %s\n",
                  (request->method() == HTTP_GET) ? "GET" : "POST");

  // Add number of arguments
  len += snprintf(buffer + len, sizeof(buffer) - len, "Arguments: %d\n", request->params());

  // Add each argument
  for (uint8_t i = 0; i < request->params(); i++) {
    const AsyncWebParameter* param = request->getParam(i);
    len += snprintf(buffer + len, sizeof(buffer) - len, "  %s: %s\n",
                    param->name().c_str(), param->value().c_str());
    if (len >= sizeof(buffer)) {
      break; // Prevent buffer overflow
    }
  }

  // Log the 404 details
  log_d("Server client on 404:\n %s", buffer);

  // Send the response
  request->send(404, "text/plain", buffer);
}

#endif