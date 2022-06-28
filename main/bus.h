#ifndef ESP_IOT_NODE_PLUS_BUS_H_
#define ESP_IOT_NODE_PLUS_BUS_H_

#include <esp_err.h>
#include "config.h"

typedef enum {
    NETWORK_UP = 0,
    NETWORK_DOWN,
    MQTT_CONNECTED,
    MQTT_DISCONNECTED,
    MODE_SET,
} event_type_t;

typedef struct
{
    event_type_t type;
    uint8_t data[BUS_EVENT_DATA_SIZE];
} event_t;

esp_err_t bus_init();

esp_err_t bus_send_event(event_type_t type, void *data, size_t size);

esp_err_t bus_receive_event(event_t *e, size_t timeout_ms);

#endif // ESP_IOT_NODE_PLUS_BUS_H_
