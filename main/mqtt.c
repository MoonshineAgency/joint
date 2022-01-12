#include "mqtt.h"
#include "bus.h"
#include "settings.h"
#include "system.h"

esp_mqtt_client_handle_t mqtt_client = NULL;
static bool connected = false;

static void handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch ((esp_mqtt_event_id_t)event_id)
    {
        case MQTT_EVENT_CONNECTED:
            connected = true;
            ESP_LOGI(TAG, "Connected to MQTT broker");
            bus_send_event(EVENT_MQTT_CONNECTED, NULL, 0);
            break;
        case MQTT_EVENT_DISCONNECTED:
            connected = false;
            ESP_LOGI(TAG, "Disconnected from MQTT broker");
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

    mqtt_client = esp_mqtt_client_init(&config);
    return esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, handler, NULL);
}

esp_err_t mqtt_connect()
{
    if (mqtt_is_connected())
        return ESP_OK;

    ESP_LOGI(TAG, "Connecting to MQTT broker [%s]...", settings.mqtt.uri);
    ESP_LOGI(TAG, "MQTT Client ID: [%s], Username: [%s]", SYSTEM_ID, settings.mqtt.username);

    return esp_mqtt_client_start(mqtt_client);
}

bool mqtt_is_connected()
{
    return connected;
}

esp_err_t mqtt_disconnect()
{
    if (!mqtt_is_connected())
        return ESP_OK;

    ESP_LOGI(TAG, "Disconnecting from MQTT broker [%s]...", settings.mqtt.uri);

    return esp_mqtt_client_stop(mqtt_client);
}
