#include "periodic_driver.h"
#include "common.h"

#define BIT_EXECUTING BIT(0)
#define BIT_STOPPED   BIT(1)

void task(void *arg)
{
    driver_t *drv = (driver_t *)arg;
    periodic_loop_callback_t loop = (periodic_loop_callback_t)drv->internal[0];
    EventGroupHandle_t group = (EventGroupHandle_t)drv->internal[1];

    uint32_t period = config_get_int(cJSON_GetObjectItem(drv->config, "period"), 1000);
    xEventGroupWaitBits(group, BIT_EXECUTING, pdFALSE, pdTRUE, portMAX_DELAY);

    TickType_t ticks = xTaskGetTickCount();
    while (xEventGroupGetBits(group) | BIT_EXECUTING)
    {
        loop(drv);
        vTaskDelayUntil(&ticks, pdMS_TO_TICKS(period));
    }

    xEventGroupSetBits(group, BIT_STOPPED);

    vTaskDelete(NULL);
}

esp_err_t periodic_driver_init(driver_t *drv)
{
    CHECK_ARG(drv);

    uint32_t stack_size = config_get_int(
        cJSON_GetObjectItem(drv->config, "stack_size"),
        CONFIG_DEFAULT_DRIVER_STACK_SIZE);
    UBaseType_t priority = config_get_int(
        cJSON_GetObjectItem(drv->config, "priority"),
        tskIDLE_PRIORITY + 1);

    drv->internal[1] = xEventGroupCreate();
    xEventGroupClearBits((EventGroupHandle_t)(drv->internal[1]), BIT_EXECUTING | BIT_STOPPED);

    ESP_LOGI(TAG, "Creating task for periodic driver %s (stack_size=%d, priority=%d)", drv->name, stack_size, priority);
    int res = xTaskCreate(task, NULL, stack_size, drv, priority, (TaskHandle_t *)&drv->context);
    if (res != pdPASS)
    {
        ESP_LOGE(TAG, "Error creating task for periodic driver %s", drv->name);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t periodic_driver_start(driver_t *drv)
{
    xEventGroupSetBits((EventGroupHandle_t)(drv->internal[1]), BIT_EXECUTING);
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
    EventGroupHandle_t group = (EventGroupHandle_t)(drv->internal[1]);

    xEventGroupClearBits(group, BIT_EXECUTING);
    xEventGroupWaitBits(group, BIT_STOPPED, pdFALSE, pdTRUE, portMAX_DELAY);

    vEventGroupDelete(group);

    return ESP_OK;
}
