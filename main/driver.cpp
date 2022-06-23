//
// Created by rus on 22.06.2022.
//
#include "driver.h"

///////////////////////////////////////////////////////////////////////////////

esp_err_t driver_t::create_task(size_t stack_size, uint32_t priority)
{
    if (xTaskCreate(driver_t::_task, nullptr, stack_size, this, priority, &_handle) != pdPASS)
    {
        ESP_LOGE(TAG, "driver_t: could not create task");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void driver_t::suspend()
{
    vTaskSuspend(_handle);
}

void driver_t::resume()
{
    vTaskResume(_handle);
}

///////////////////////////////////////////////////////////////////////////////

void periodic_driver_t::_run()
{
    xEventGroupWaitBits(_state, STATE_BIT_EXECUTING, pdFALSE, pdTRUE, portMAX_DELAY);

    uint32_t ticks = xTaskGetTickCount();
    while (xEventGroupGetBits(_state) | STATE_BIT_EXECUTING)
    {
        loop();
        vTaskDelayUntil(&ticks, pdMS_TO_TICKS(_config["period"]));
    }

    vTaskDelete(nullptr);
}

esp_err_t periodic_driver_t::init()
{
    _state = xEventGroupCreate();
    xEventGroupClearBits(_state, STATE_BIT_EXECUTING);
    CHECK(create_task(_config["stack_size"], _config["priority"]));

    return ESP_OK;
}

esp_err_t periodic_driver_t::start()
{
    xEventGroupSetBits(_state, STATE_BIT_EXECUTING);
    return ESP_OK;
}

esp_err_t periodic_driver_t::stop()
{
    xEventGroupClearBits(_state, STATE_BIT_EXECUTING);
    return ESP_OK;
}

esp_err_t periodic_driver_t::done()
{
    xEventGroupClearBits(_state, STATE_BIT_EXECUTING);
    vEventGroupDelete(_state);
    return ESP_OK;
}
