#include "node.h"
#include "bus.h"
#include "network.h"
#include "mqtt.h"
#include "system.h"

static void task(void *arg)
{
    ESP_LOGI(TAG, "Normal mode task started");

    event_t e;

    SYSTEM_CHECK(network_init());
    SYSTEM_CHECK(mqtt_init());
    SYSTEM_CHECK(node_init());

    while (1)
    {
        if (bus_receive_event(&e, 1000) != ESP_OK)
            continue;
        switch (e.type)
        {
            case EVENT_NETWORK_UP:
                if (mqtt_is_connected())
                    mqtt_disconnect();
                SYSTEM_CHECK(mqtt_connect());
                break;
            case EVENT_MQTT_CONNECTED:
                SYSTEM_CHECK(node_online());
                break;
            case EVENT_MQTT_DISCONNECTED:
                SYSTEM_CHECK(node_offline());
                break;
            case EVENT_NETWORK_DOWN:
                SYSTEM_CHECK(node_offline());
                mqtt_disconnect();
                break;
            default:
                break;
        }
    }
}

esp_err_t normal_mode_start()
{
    ESP_LOGI(TAG, "Booting...");

    // Create main task
    if (xTaskCreate(task, "__main__", MAIN_TASK_STACK_SIZE,
                    NULL, MAIN_TASK_PRIORITY, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create main task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t node_init()
{
    ESP_LOGI(TAG, "Initializing node %s...", SYSTEM_ID);

    system_set_mode(MODE_OFFLINE);
    return ESP_OK;
}

esp_err_t node_online()
{
    ESP_LOGI(TAG, "Node goes online...");

    system_set_mode(MODE_ONLINE);
    return ESP_OK;
}

esp_err_t node_offline()
{
    ESP_LOGI(TAG, "Node goes offline...");

    system_set_mode(MODE_OFFLINE);
    return ESP_OK;
}
