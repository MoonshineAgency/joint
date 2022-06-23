#ifndef ESP_IOT_NODE_PLUS_MQTT_H_
#define ESP_IOT_NODE_PLUS_MQTT_H_

#include "common.h"
#include <string>
#include <mqtt_client.h>
#include <cJSON.h>

class mqtt_client_t
{
private:
    esp_mqtt_client_handle_t _handle;
    bool _connected;
    std::string _main_topic;

    static mqtt_client_t *_instance;

    static void _handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

    mqtt_client_t()
        :_handle(nullptr), _connected(false)
    {}

public:
    static mqtt_client_t *get()
    {
        if (_instance == nullptr)
            _instance = new mqtt_client_t();
        return _instance;
    }

    mqtt_client_t(const mqtt_client_t&) = delete;
    mqtt_client_t &operator=(const mqtt_client_t&) = delete;

    esp_mqtt_client_handle_t handle() { return _handle; }

    esp_err_t init();
    esp_err_t connect();
    esp_err_t disconnect();

    bool connected() const { return _connected; }

    int publish(const char *topic, const char *data, int len, int qos, int retain);
    int publish(const std::string &topic, const std::string &data, int qos, int retain);

    int publish_json(const std::string &subtopic, cJSON *json, int qos, int retain);
};

#endif // ESP_IOT_NODE_PLUS_MQTT_H_
