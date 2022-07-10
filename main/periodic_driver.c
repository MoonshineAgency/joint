#include "periodic_driver.h"
#include "common.h"

#define BIT_RUN BIT(0)

void task(void *arg)
{
    driver_t *drv = (driver_t *)arg;
    periodic_loop_callback_t loop = (periodic_loop_callback_t)drv->internal[0];
    EventGroupHandle_t group = (EventGroupHandle_t)drv->internal[1];

    uint32_t period = config_get_int(cJSON_GetObjectItem(drv->config, "period"), 1000);
    xEventGroupWaitBits(group, BIT_RUN, pdFALSE, pdTRUE, portMAX_DELAY);

    TickType_t ticks = xTaskGetTickCount();
    while (true)
    {
        loop(drv);
        vTaskDelayUntil(&ticks, pdMS_TO_TICKS(period));
    }
}

esp_err_t periodic_driver_init(driver_t *drv)
{
    CHECK_ARG(drv);

    uint32_t stack_size = config_get_int(cJSON_GetObjectItem(drv->config, "stack_size"), CONFIG_DEFAULT_DRIVER_STACK_SIZE);
    UBaseType_t priority = config_get_int(cJSON_GetObjectItem(drv->config, "priority"), tskIDLE_PRIORITY + 1);

    drv->internal[1] = xEventGroupCreate();
    xEventGroupClearBits((EventGroupHandle_t)(drv->internal[1]), BIT_RUN);

    ESP_LOGI(TAG, "Creating task for periodic driver %s (stack_size=%d, priority=%d)", drv->name, stack_size, priority);
    int res = xTaskCreatePinnedToCore(task, NULL, stack_size, drv, priority, (TaskHandle_t *)&drv->context, APP_CPU_NUM);
    if (res != pdPASS)
    {
        ESP_LOGE(TAG, "Error creating task for periodic driver %s", drv->name);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t periodic_driver_start(driver_t *drv)
{
    xEventGroupSetBits((EventGroupHandle_t)(drv->internal[1]), BIT_RUN);
    return ESP_OK;
}

esp_err_t periodic_driver_suspend(driver_t *drv)
{
    vTaskSuspend((TaskHandle_t)drv->context);
    return ESP_OK;
}

esp_err_t periodic_driver_resume(driver_t *drv)
{
    vTaskResume((TaskHandle_t)drv->context);
    return ESP_OK;
}

esp_err_t periodic_driver_stop(driver_t *drv)
{
    vTaskDelete((TaskHandle_t)drv->context);
    vEventGroupDelete((EventGroupHandle_t)(drv->internal[1]));

    return ESP_OK;
}
