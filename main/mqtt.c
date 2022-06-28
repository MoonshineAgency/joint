#include "mqtt.h"
#include "common.h"
#include "bus.h"
#include "settings.h"
#include "system.h"
#include "cvector.h"

#define MAX_TOPIC_LEN (sizeof(settings.node.name) + 100)

static bool connected = false;
static esp_mqtt_client_handle_t handle = NULL;

static char *msg_data = NULL;
static size_t msg_data_size = 0;
static char *msg_topic = NULL;

typedef struct
{
    char topic[MAX_TOPIC_LEN];
    mqtt_callback_t callback;
} subscription_t;

static cvector_vector_type(subscription_t) subs = NULL;

static void on_data(esp_mqtt_event_handle_t event)
{
    if (event->current_data_offset == 0)
    {
        // head event
        // save topic
        msg_topic = malloc(event->topic_len + 1);
        memcpy(msg_topic, event->topic, event->topic_len);
        msg_topic[event->topic_len] = 0;

        // save data
        msg_data = malloc(event->total_data_len + 1);
        memcpy(msg_data, event->data, event->data_len);
        msg_data[event->total_data_len] = 0;

        msg_data_size = event->data_len;
    }
    else
    {
        // tail event, append data
        memcpy(msg_data + msg_data_size, msg_data, event->data_len);
        msg_data_size += event->data_len;
    }

    if (msg_data_size >= event->data_len)
    {
        // time to callback
        for (size_t i = 0; i < cvector_size(subs); i++)
            if (!strncmp(msg_topic, subs[i].topic, MAX_TOPIC_LEN - 1))
                subs[i].callback(msg_topic, msg_data, msg_data_size + 1);

        // free memory
        free(msg_data);
        free(msg_topic);
    }
}

static void handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch ((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_CONNECTED:
            connected = true;
            ESP_LOGI(TAG, "Connected to MQTT broker");
            bus_send_event(MQTT_CONNECTED, NULL, 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            connected = false;
            ESP_LOGI(TAG, "Disconnected from MQTT broker");
            bus_send_event(MQTT_DISCONNECTED, NULL, 0);
            break;
        case MQTT_EVENT_DATA:
            on_data((esp_mqtt_event_handle_t)event_data);
        default:
            break;
    }
}

inline void full_topic(char *buf, const char *subtopic)
{
    snprintf(buf, MAX_TOPIC_LEN, "%s/%s", settings.node.name, subtopic);
}

////////////////////////////////////////////////////////////////////////////////

esp_mqtt_client_handle_t mqtt_client()
{
    return handle;
}

esp_err_t mqtt_init()
{
    ESP_LOGI(TAG, "Initializing MQTT client with ID: '%s', username: '%s'", SYSTEM_ID, settings.mqtt.username);

    esp_mqtt_client_config_t config;
    memset(&config, 0, sizeof(config));

    config.uri = settings.mqtt.uri;
    config.username = settings.mqtt.username;
    config.password = settings.mqtt.password;
    config.client_id = SYSTEM_ID;

    handle = esp_mqtt_client_init(&config);
    return esp_mqtt_client_register_event(handle, ESP_EVENT_ANY_ID, handler, NULL);
}

esp_err_t mqtt_connect()
{
    ESP_LOGI(TAG, "Connecting to MQTT broker '%s'...", settings.mqtt.uri);

    return esp_mqtt_client_start(handle);
}

esp_err_t mqtt_disconnect()
{
    ESP_LOGI(TAG, "Disconnecting from MQTT broker '%s'...", settings.mqtt.uri);

    return esp_mqtt_client_stop(handle);
}

bool mqtt_connected()
{
    return connected;
}

int mqtt_publish(const char *topic, const char *data, int len, int qos, int retain)
{
    return esp_mqtt_client_publish(handle, topic, data, len, qos, retain);
}

int mqtt_publish_json(const char *topic, const cJSON *json, int qos, int retain)
{
    char *buf = cJSON_PrintUnformatted(json);
    int res = mqtt_publish(topic, buf, (int)strlen(buf), qos, retain);
    cJSON_free(buf);

    return res;
}

int mqtt_publish_subtopic(const char *subtopic, const char *data, int len, int qos, int retain)
{
    char topic[MAX_TOPIC_LEN] = { 0 };
    full_topic(topic, subtopic);

    return esp_mqtt_client_publish(handle, topic, data, len, qos, retain);
}

int mqtt_publish_json_subtopic(const char *subtopic, const cJSON *json, int qos, int retain)
{
    char topic[MAX_TOPIC_LEN] = { 0 };
    full_topic(topic, subtopic);

    return mqtt_publish_json(topic, json, qos, retain);
}

int mqtt_subscribe(const char *topic, mqtt_callback_t cb, int qos)
{
    bool subscribed = false;
    for (size_t i = 0; i < cvector_size(subs); i++)
        if (!strncmp(topic, subs[i].topic, MAX_TOPIC_LEN - 1))
        {
            if (cb == subs[i].callback)
                return -1;
            subscribed = true;
        }

    subscription_t s = {
        .topic = { 0 },
        .callback = cb
    };
    strncpy(s.topic, topic, sizeof(s.topic) - 1);
    cvector_push_back(subs, s);

    return subscribed ? 0 : esp_mqtt_client_subscribe(handle, topic, qos);
}

int mqtt_subscribe_subtopic(const char *subtopic, mqtt_callback_t cb, int qos)
{
    char topic[MAX_TOPIC_LEN] = { 0 };
    full_topic(topic, subtopic);

    return mqtt_subscribe(topic, cb, qos);
}

void mqtt_unsubscribe(const char *topic, mqtt_callback_t cb)
{
    for (size_t i = 0; i < cvector_size(subs); i++)
        if (!strncmp(topic, subs[i].topic, MAX_TOPIC_LEN - 1) && cb == subs[i].callback)
        {
            cvector_erase(subs, i);
            return;
        }
}

void mqtt_unsubscribe_subtopic(const char *subtopic, mqtt_callback_t cb)
{
    char topic[MAX_TOPIC_LEN] = { 0 };
    full_topic(topic, subtopic);

    mqtt_unsubscribe(topic, cb);
}
