#include "driver.h"
#include "common.h"
#include "node.h"

#define ERR_INVALID_STATE "[%s] Driver in invalid state"

#define DRIVER_TIMEOUT 1000

static void driver_task(void *arg)
{
    driver_t *self = (driver_t *)arg;
    esp_err_t r;

    xEventGroupClearBits(self->eg, DRIVER_BIT_INITIALIZED | DRIVER_BIT_RUNNING);

    if (self->on_init)
    {
        r = self->on_init(self);
        if (r != ESP_OK)
        {
            self->state = DRIVER_INVALID;
            ESP_LOGE(TAG, "[%s] Error initializing driver: %d (%s)", self->name, r, esp_err_to_name(r));
            goto exit;
        }
    }

    self->state = DRIVER_INITIALIZED;
    xEventGroupSetBits(self->eg, DRIVER_BIT_INITIALIZED);

    xEventGroupWaitBits(self->eg, DRIVER_BIT_START, pdFALSE, pdTRUE, portMAX_DELAY);
    if (self->on_start)
    {
        r = self->on_start(self);
        if (r != ESP_OK)
        {
            self->state = DRIVER_INVALID;
            ESP_LOGE(TAG, "[%s] Error starting driver: %d (%s)", self->name, r, esp_err_to_name(r));
            goto exit;
        }
    }

    self->state = DRIVER_RUNNING;
    xEventGroupSetBits(self->eg, DRIVER_BIT_RUNNING);
    vTaskDelay(1);

    self->task(self);

    if (self->on_stop)
    {
        r = self->on_stop(self);
        if (r != ESP_OK)
        {
            self->state = DRIVER_INVALID;
            ESP_LOGE(TAG, "[%s] Error stopping driver: %d (%s)", self->name, r, esp_err_to_name(r));
            goto exit;
        }
    }

    self->state = DRIVER_FINISHED;
    xEventGroupSetBits(self->eg, DRIVER_BIT_STOPPED);

exit:
    self->handle = NULL;
    vTaskDelete(NULL);
}

////////////////////////////////////////////////////////////////////////////////

esp_err_t driver_init(driver_t *drv, const char *config, size_t cfg_len)
{
    CHECK_ARG(drv);

    if (drv->state != DRIVER_NEW && drv->state != DRIVER_FINISHED && drv->state != DRIVER_INVALID)
    {
        ESP_LOGE(TAG, ERR_INVALID_STATE, drv->name);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t r = ESP_OK;

    if (drv->config)
    {
        cJSON_Delete(drv->config);
        drv->config = NULL;
    }

    if (config && cfg_len)
        drv->config = cJSON_ParseWithLength(config, cfg_len);

    if (drv->defconfig)
    {
        if (!drv->config)
        {
            ESP_LOGW(TAG, "[%s] Invalid or empty driver config, using default", drv->name);
            drv->config = cJSON_Parse(drv->defconfig);
        }

        char *buf = cJSON_PrintUnformatted(drv->config);
        ESP_LOGI(TAG, "[%s] Driver config: %s", drv->name, buf);
        cJSON_free(buf);
    }

    if (drv->eg)
        vEventGroupDelete(drv->eg);

    drv->eg = xEventGroupCreate();
    if (!drv->eg)
    {
        ESP_LOGE(TAG, "[%s] Error creating event group for driver", drv->name);
        r = ESP_ERR_NO_MEM;
        goto exit;
    }
    xEventGroupClearBits(drv->eg, DRIVER_BIT_INITIALIZED | DRIVER_BIT_RUNNING | DRIVER_BIT_START);


    if (drv->handle)
    {
        eTaskState state = eTaskGetState(drv->handle);
        if (state != eDeleted && state != eInvalid)
            vTaskDelete(drv->handle);
    }

    ESP_LOGI(TAG, "[%s] Creating driver task (stack_size=%" PRIu32 ", priority=%d)", drv->name, drv->stack_size, drv->priority);
    int res = xTaskCreatePinnedToCore(driver_task, NULL, drv->stack_size, drv, drv->priority, &drv->handle, APP_CPU_NUM);
    if (res != pdPASS)
    {
        ESP_LOGE(TAG, "[%s] Error creating task for driver", drv->name);
        r = ESP_ERR_NO_MEM;
        goto exit;
    }

    // wait while driver init
    EventBits_t bits = xEventGroupWaitBits(drv->eg, DRIVER_BIT_INITIALIZED, pdFALSE, pdTRUE, pdMS_TO_TICKS(DRIVER_TIMEOUT));
    if (!(bits & DRIVER_BIT_INITIALIZED))
    {
        ESP_LOGE(TAG, "[%s] Driver has not been initialized due to error or timeout", drv->name);
        r = ESP_ERR_TIMEOUT;
        goto exit;
    }

exit:
    if (r == ESP_OK)
    {
        drv->state = DRIVER_INITIALIZED;
        ESP_LOGI(TAG, "[%s] Driver initialized", drv->name);
    }
    else
    {
        drv->state = DRIVER_INVALID;
        ESP_LOGE(TAG, "[%s] Error initializing driver: %d (%s)", drv->name, r, esp_err_to_name(r));
    }
    
    return r;
}

esp_err_t driver_start(driver_t *drv)
{
    CHECK_ARG(drv);

    if (drv->state != DRIVER_INITIALIZED)
    {
        ESP_LOGE(TAG, ERR_INVALID_STATE, drv->name);
        return ESP_ERR_INVALID_STATE;
    }

    xEventGroupSetBits(drv->eg, DRIVER_BIT_START);

    EventBits_t bits = xEventGroupWaitBits(drv->eg, DRIVER_BIT_RUNNING, pdFALSE, pdTRUE, pdMS_TO_TICKS(DRIVER_TIMEOUT));
    if (!(bits & DRIVER_BIT_RUNNING))
    {
        ESP_LOGE(TAG, "[%s] Driver has not been started due to error or timeout", drv->name);
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "[%s] Driver started", drv->name);

    return ESP_OK;
}

esp_err_t driver_stop(driver_t *drv)
{
    CHECK_ARG(drv);

    if (drv->state != DRIVER_RUNNING && drv->state != DRIVER_INVALID)
    {
        ESP_LOGE(TAG, ERR_INVALID_STATE, drv->name);
        return ESP_ERR_INVALID_STATE;
    }

    xEventGroupClearBits(drv->eg, DRIVER_BIT_START);

    EventBits_t bits = xEventGroupWaitBits(drv->eg, DRIVER_BIT_STOPPED, pdFALSE, pdTRUE, pdMS_TO_TICKS(DRIVER_TIMEOUT));
    if (!(bits & DRIVER_BIT_STOPPED))
    {
        ESP_LOGE(TAG, "[%s] Driver has not been stopped due to error or timeout", drv->name);
        return ESP_ERR_TIMEOUT;
    }

    drv->handle = NULL;

    ESP_LOGI(TAG, "[%s] Driver stopped", drv->name);

    return ESP_OK;
}

void driver_send_device_update(driver_t *drv, const device_t *dev)
{
    driver_event_t e = {
        .type = DRV_EVENT_DEVICE_UPDATED,
        .sender = drv,
        .dev = *dev
    };
    xQueueSend(drv->event_queue, &e, 0);
}

void driver_send_device_add(driver_t *drv, const device_t *dev)
{
    driver_event_t e = {
        .type = DRV_EVENT_DEVICE_ADDED,
        .sender = drv,
        .dev = *dev
    };
    xQueueSend(drv->event_queue, &e, 0);
}

void driver_send_device_remove(driver_t *drv, const device_t *dev)
{
    driver_event_t e = {
        .type = DRV_EVENT_DEVICE_REMOVED,
        .sender = drv,
        .dev = *dev
    };
    xQueueSend(drv->event_queue, &e, 0);
}

int driver_config_get_int(cJSON *item, int def)
{
    return cJSON_IsNumber(item) ? (int)cJSON_GetNumberValue(item) : def;
}

float driver_config_get_float(cJSON *item, float def)
{
    return cJSON_IsNumber(item) ? (float)cJSON_GetNumberValue(item) : def;
}

gpio_num_t driver_config_get_gpio(cJSON *item, gpio_num_t def)
{
    int res = driver_config_get_int(item, def);
    return res >= GPIO_NUM_MAX ? def : res;
}

bool driver_config_get_bool(cJSON *item, bool def)
{
    return cJSON_IsBool(item) ? cJSON_IsTrue(item) : def;
}
