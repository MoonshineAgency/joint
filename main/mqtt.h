#ifndef ESP_IOT_NODE_PLUS_MQTT_H
#define ESP_IOT_NODE_PLUS_MQTT_H

#include "common.h"
#include <mqtt_client.h>

#ifdef __cplusplus
extern "C" {
#endif

extern esp_mqtt_client_handle_t mqtt_client;

esp_err_t mqtt_init();

esp_err_t mqtt_connect();

bool mqtt_is_connected();

esp_err_t mqtt_disconnect();

#ifdef __cplusplus
}
#endif

#endif //ESP_IOT_NODE_PLUS_MQTT_H
