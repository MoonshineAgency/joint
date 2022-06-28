#include "normal_mode.h"
#include "common.h"
#include "system.h"
#include "bus.h"
#include "node.h"
#include "mqtt.h"
#include "wifi.h"
#include "webserver.h"

static void task(void *arg)
{
    ESP_LOGI(TAG, "Normal mode task started");

    SYSTEM_CHECK(wifi_init());
    SYSTEM_CHECK(mqtt_init());
    SYSTEM_CHECK(node_init());

    esp_err_t res = ESP_OK;
    event_t e;

    while (true)
    {
        if (bus_receive_event(&e, 1000) != ESP_OK)
            continue;
        switch (e.type)
        {
            case NETWORK_UP:
                if (mqtt_connected())
                    mqtt_disconnect();
                SYSTEM_CHECK(mqtt_connect());
                res = webserver_restart();
                if (res != ESP_OK)
                    ESP_LOGW(TAG, "Could not restart webserver");
                break;
            case MQTT_CONNECTED:
                SYSTEM_CHECK(node_online());
                break;
            case MQTT_DISCONNECTED:
                SYSTEM_CHECK(node_offline());
                break;
            case NETWORK_DOWN:
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
    if (xTaskCreate(task, "__main__", MAIN_TASK_STACK_SIZE, NULL, MAIN_TASK_PRIORITY, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create main task");
        return ESP_FAIL;
    }

    system_set_mode(MODE_BOOT);

    return ESP_OK;
}
