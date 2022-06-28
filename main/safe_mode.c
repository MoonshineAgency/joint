#include "safe_mode.h"
#include "common.h"
#include "system.h"
#include "bus.h"
#include "wifi.h"
#include "webserver.h"

static void task(void *arg)
{
    ESP_LOGI(TAG, "Safe mode task started");
    system_set_mode(MODE_SAFE);

    esp_err_t res;

    ESP_ERROR_CHECK(wifi_init());

    event_t e;
    while (true)
    {
        if (bus_receive_event(&e, 1000) != ESP_OK)
            continue;
        switch (e.type)
        {
            case NETWORK_DOWN:
                ESP_LOGW(TAG, "Network is down");
                break;
            case NETWORK_UP:
                ESP_LOGI(TAG, "Network is up");
                res = webserver_restart();
                if (res != ESP_OK)
                    ESP_LOGW(TAG, "Could not restart webserver");
                break;
            default:
                break;
        }
    }
}

esp_err_t safe_mode_start()
{
    system_set_mode(MODE_BOOT);
    ESP_LOGI(TAG, "Booting in safe mode...");

    // Create safe_mode task
    if (xTaskCreate(task, "__safemode__", SAFEMODE_TASK_STACK_SIZE, NULL, SAFEMODE_TASK_PRIORITY, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create safe mode main task");
        ESP_ERROR_CHECK(ESP_FAIL);
    }

    return ESP_OK;
}
