#include "safemode.h"
#include "system.h"
#include "bus.h"
#include "network.h"

static void task(void *arg)
{
    ESP_LOGI(TAG, "Safe mode task started");

    ESP_ERROR_CHECK(network_init());

    event_t e;
    while (1)
    {
        if (bus_receive_event(&e, 1000) != ESP_OK)
            continue;
        switch (e.type)
        {
            case EVENT_NETWORK_DOWN:
                ESP_LOGW(TAG, "Network is down");
                break;
            case EVENT_NETWORK_UP:
                ESP_LOGI(TAG, "Network is up");
                break;
            default:
                break;
        }
    }
}

esp_err_t safe_mode_start()
{
    system_set_mode(MODE_SAFE);
    ESP_LOGI(TAG, "Booting in safe mode...");

    // Create safe_mode task
    if (xTaskCreate(task, "__safemode__", SAFEMODE_TASK_STACK_SIZE,
                    NULL, SAFEMODE_TASK_PRIORITY, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "Could not create safe mode main task");
        ESP_ERROR_CHECK(ESP_FAIL);
    }

    return ESP_OK;
}
