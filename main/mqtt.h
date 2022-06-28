#ifndef ESP_IOT_NODE_PLUS_MQTT_H_
#define ESP_IOT_NODE_PLUS_MQTT_H_

#include <esp_err.h>
#include <mqtt_client.h>
#include <cJSON.h>

typedef void (*mqtt_callback_t)(const char *topic, const char *data, size_t data_len);

esp_mqtt_client_handle_t mqtt_client();

esp_err_t mqtt_init();
esp_err_t mqtt_connect();
esp_err_t mqtt_disconnect();
bool mqtt_connected();

int mqtt_publish(const char *topic, const char *data, int len, int qos, int retain);
int mqtt_publish_json(const char *topic, const cJSON *json, int qos, int retain);

int mqtt_publish_subtopic(const char *subtopic, const char *data, int len, int qos, int retain);
int mqtt_publish_json_subtopic(const char *subtopic, const cJSON *json, int qos, int retain);

int mqtt_subscribe(const char *topic, mqtt_callback_t cb, int qos);
int mqtt_subscribe_subtopic(const char *subtopic, mqtt_callback_t cb, int qos);

void mqtt_unsubscribe(const char *topic, mqtt_callback_t cb);
void mqtt_unsubscribe_subtopic(const char *subtopic, mqtt_callback_t cb);

#endif // ESP_IOT_NODE_PLUS_MQTT_H_
