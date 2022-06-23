#include "mqtt.h"
#include "bus.h"
#include "settings.h"
#include "system.h"

mqtt_client_t *mqtt_client_t::_instance;

extern "C"
void mqtt_client_t::_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    auto client = static_cast<mqtt_client_t *>(arg);
    switch ((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_CONNECTED:
            client->_connected = true;
            ESP_LOGI(TAG, "Connected to MQTT broker");
            bus::send_event(bus::MQTT_CONNECTED, nullptr, 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            client->_connected = false;
            ESP_LOGI(TAG, "Disconnected from MQTT broker");
            bus::send_event(bus::MQTT_DISCONNECTED, nullptr, 0);
            break;
        default:
            break;
    }
}

esp_err_t mqtt_client_t::init()
{
    ESP_LOGI(TAG, "Initializing MQTT client with ID: '%s', username: '%s'", SYSTEM_ID, settings::get()->data.mqtt.username);

    esp_mqtt_client_config_t config;
    memset(&config, 0, sizeof(config));

    config.uri = settings::get()->data.mqtt.uri;
    config.username = settings::get()->data.mqtt.username;
    config.password = settings::get()->data.mqtt.password;
    config.client_id = SYSTEM_ID;

    _main_topic = settings::get()->data.node.name;
    _main_topic += "/";

    _handle = esp_mqtt_client_init(&config);
    return esp_mqtt_client_register_event(_handle, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID), _handler, this);
}

esp_err_t mqtt_client_t::connect()
{
    ESP_LOGI(TAG, "Connecting to MQTT broker '%s'...", settings::get()->data.mqtt.uri);

    return esp_mqtt_client_start(_handle);
}

esp_err_t mqtt_client_t::disconnect()
{
    ESP_LOGI(TAG, "Disconnecting from MQTT broker '%s'...", settings::get()->data.mqtt.uri);

    return esp_mqtt_client_stop(_handle);
}

int mqtt_client_t::publish(const char *topic, const char *data, int len, int qos, int retain)
{
    return esp_mqtt_client_publish(_handle, topic, data, len, qos, retain);
}

int mqtt_client_t::publish(const std::string &topic, const std::string &data, int qos, int retain)
{
    return esp_mqtt_client_publish(_handle, topic.c_str(), data.c_str(), (int)data.size(), qos, retain);
}

int mqtt_client_t::publish_json(const std::string &subtopic, cJSON *json, int qos, int retain)
{
    auto buf = cJSON_PrintUnformatted(json);
    auto res = publish(_main_topic + subtopic, buf, qos, retain);
    cJSON_free(buf);

    return res;
}
