#include "normal_mode.h"
#include "common.h"
#include "system.h"
#include "bus.h"
#include "node.h"
#include "mqtt.h"
#include "wifi.h"
#include "webserver.h"
#include "system_clock.h"
#include "settings.h"

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
                system_clock_sntp_init();
                mqtt_disconnect();
                mqtt_connect();
                res = webserver_restart();
                if (res != ESP_OK)
                    ESP_LOGW(TAG, "Could not restart webserver");
                break;
            case MQTT_CONNECTED:
                node_online();
                break;
            case MQTT_DISCONNECTED:
                node_offline();
                break;
            case SETTINGS_RESET:
                settings_reset();
                esp_restart();
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
