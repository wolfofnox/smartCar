#ifndef WIFI_STA_HANDLERS_H
#define WIFI_STA_HANDLERS_H

#include <stdint.h>

// Control packet types for binary WebSocket protocol
typedef enum {
    VALUE_NONE,
    CONTROL_SPEED,
    CONTROL_STEERING,
    CONTROL_TOP_SERVO,
    CONFIG_STEERING_MAX_PULSEWIDTH,
    CONFIG_STEERING_MIN_PULSEWIDTH,
    CONFIG_TOP_MAX_PULSEWIDTH,
    CONFIG_TOP_MIN_PULSEWIDTH,
    CONFIG_WS_TIMEOUT
} ws_value_type_t;

typedef enum {
    EVENT_NONE,
    EVENT_TIMEOUT,
    EVENT_ESTOP,
    EVENT_REVERT_SETTINGS
} ws_event_type_t;

// Binary control packet structure
typedef struct __attribute__((packed)) {
    ws_value_type_t type;  // Control type (1 byte)
    int16_t value;       // Control value (2 bytes)
} ws_control_packet_t;

void set_handlers();

#endif // WIFI_STA_HANDLERS_H