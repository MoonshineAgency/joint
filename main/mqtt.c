#include "mqtt.h"
#include "bus.h"
#include "settings.h"
#include "system.h"
#include <mqtt_client.h>

static esp_mqtt_client_handle_t client = NULL;
static bool connected = false;

static void handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch ((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_CONNECTED:
            connected = true;
            bus_send_event(EVENT_MQTT_CONNECTED, NULL, 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            connected = false;
            bus_send_event(EVENT_MQTT_DISCONNECTED, NULL, 0);
            break;
        default:
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////

esp_err_t mqtt_init()
{
    ESP_LOGI(TAG, "Initializing MQTT client...");

    esp_mqtt_client_config_t config = {
            .uri = settings.mqtt.uri,
            .client_id = SYSTEM_ID,
            .username = settings.mqtt.username,
            .password = settings.mqtt.password,
    };
    ESP_LOGI(TAG, "--------------------------------------------------");
    ESP_LOGI(TAG, "URI: %s", config.uri);
    ESP_LOGI(TAG, "Client ID: %s", config.client_id);
    ESP_LOGI(TAG, "User: %s", config.username);
    ESP_LOGI(TAG, "Password: %s", config.password);

    client = esp_mqtt_client_init(&config);
    return esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, handler, NULL);
}

esp_err_t mqtt_connect()
{
    return mqtt_is_connected()
        ? ESP_OK
        : esp_mqtt_client_start(client);
}

bool mqtt_is_connected()
{
    return connected;
}

esp_err_t mqtt_disconnect()
{
    return mqtt_is_connected()
           ? esp_mqtt_client_stop(client)
           : ESP_OK;
}
