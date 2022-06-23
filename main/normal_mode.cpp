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

    SYSTEM_CHECK(wifi::init());
    SYSTEM_CHECK(mqtt_client_t::get()->init());
    SYSTEM_CHECK(node::init());

    esp_err_t res = ESP_OK;
    bus::event_t e;

    while (true)
    {
        if (bus::receive_event(&e, 1000) != ESP_OK)
            continue;
        switch (e.type)
        {
            case bus::NETWORK_UP:
                if (mqtt_client_t::get()->connected())
                    mqtt_client_t::get()->disconnect();
                SYSTEM_CHECK(mqtt_client_t::get()->connect());
                res = webserver_restart();
                if (res != ESP_OK)
                    ESP_LOGW(TAG, "Could not restart webserver");
                break;
            case bus::MQTT_CONNECTED:
                SYSTEM_CHECK(node::online());
                break;
            case bus::MQTT_DISCONNECTED:
                SYSTEM_CHECK(node::offline());
                break;
            case bus::NETWORK_DOWN:
                SYSTEM_CHECK(node::offline());
                mqtt_client_t::get()->disconnect();
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
    if (xTaskCreate(task, "__main__", MAIN_TASK_STACK_SIZE, nullptr, MAIN_TASK_PRIORITY, nullptr) != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create main task");
        return ESP_FAIL;
    }

    sys::set_mode(sys::MODE_BOOT);

    return ESP_OK;
}
