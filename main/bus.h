#ifndef ESP_IOT_NODE_PLUS_BUS_H_
#define ESP_IOT_NODE_PLUS_BUS_H_

#include "common.h"

namespace bus {

enum event_type_t {
    NETWORK_UP = 0,
    NETWORK_DOWN,
    MQTT_CONNECTED,
    MQTT_DISCONNECTED,
    MODE_SET,
};

struct event_t
{
    event_type_t type;
    uint8_t data[BUS_EVENT_DATA_SIZE] = { 0 };
};

esp_err_t init();

esp_err_t send_event(event_type_t type, void *data, size_t size);

esp_err_t receive_event(event_t *e, size_t timeout_ms);

} // namespace bus

#endif // ESP_IOT_NODE_PLUS_BUS_H_
