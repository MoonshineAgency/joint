#ifndef ESP_IOT_NODE_PLUS_MQTT_H
#define ESP_IOT_NODE_PLUS_MQTT_H

#include "common.h"

esp_err_t mqtt_init();

esp_err_t mqtt_connect();

bool mqtt_is_connected();

esp_err_t mqtt_disconnect();

#endif //ESP_IOT_NODE_PLUS_MQTT_H
