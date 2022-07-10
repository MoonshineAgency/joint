#include "driver.h"
#include "common.h"

#define ERR_INVALID_STATE "Driver %s is in invalid state"

static const char *state_names[] = {
    [DRIVER_NEW]         = "new",
    [DRIVER_INITIALIZED] = "initialized",
    [DRIVER_RUNNING]     = "running",
    [DRIVER_SUSPENDED]   = "suspended",
    [DRIVER_FINISHED]    = "finished",
    [DRIVER_INVALID]     = "invalid",
};

esp_err_t driver_init(driver_t *drv, const char *config, size_t cfg_len)
{
    CHECK_ARG(drv);

    if (drv->state != DRIVER_NEW && drv->state != DRIVER_FINISHED && drv->state != DRIVER_INVALID)
    {
        ESP_LOGE(TAG, ERR_INVALID_STATE, drv->name);
        return ESP_ERR_INVALID_STATE;
    }

    if (drv->config)
    {
        cJSON_Delete(drv->config);
        drv->config = NULL;
    }

    if (config)
    {
        drv->config = cJSON_ParseWithLength(config, cfg_len);
        if (!drv->config)
            // FIXME: use defconfig
            ESP_LOGW(TAG, "Invalid config for driver %s, using default", drv->name);
    }

    esp_err_t res = ESP_OK;
    if (drv->init)
        res = drv->init(drv);
    if (res == ESP_OK)
    {
        drv->state = DRIVER_INITIALIZED;
        ESP_LOGI(TAG, "Driver %s initialized", drv->name);
    }
    else
    {
        ESP_LOGE(TAG, "Error initializing driver %s: %d (%s)", drv->name, res, esp_err_to_name(res));
        drv->state = DRIVER_INVALID;
    }
    
    return res;
}

esp_err_t driver_start(driver_t *drv)
{
    CHECK_ARG(drv);

    if (drv->state != DRIVER_INITIALIZED)
    {
        ESP_LOGE(TAG, ERR_INVALID_STATE, drv->name);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t res = ESP_OK;
    if (drv->start)
        res = drv->start(drv);
    if (res == ESP_OK)
    {
        ESP_LOGI(TAG, "Driver %s started", drv->name);
        drv->state = DRIVER_RUNNING;
    }
    else
    {
        ESP_LOGE(TAG, "Error starting driver %s: %d (%s)", drv->name, res, esp_err_to_name(res));
        drv->state = DRIVER_INVALID;
    }

    return res;
}

esp_err_t driver_suspend(driver_t *drv)
{
    CHECK_ARG(drv);

    if (drv->state != DRIVER_RUNNING)
    {
        ESP_LOGE(TAG, ERR_INVALID_STATE, drv->name);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t res = ESP_OK;
    if (drv->suspend)
        res = drv->suspend(drv);
    if (res == ESP_OK)
    {
        ESP_LOGI(TAG, "Driver %s suspended", drv->name);
        drv->state = DRIVER_SUSPENDED;
    }
    else
    {
        ESP_LOGE(TAG, "Error suspending driver %s: %d (%s)", drv->name, res, esp_err_to_name(res));
        drv->state = DRIVER_INVALID;
    }

    return res;
}

esp_err_t driver_resume(driver_t *drv)
{
    CHECK_ARG(drv);

    ESP_LOGI(TAG, "Resuming driver %s...", drv->name);
    if (drv->state != DRIVER_SUSPENDED)
    {
        ESP_LOGE(TAG, ERR_INVALID_STATE, drv->name);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t res = ESP_OK;
    if (drv->resume)
        res = drv->resume(drv);
    if (res == ESP_OK)
    {
        ESP_LOGI(TAG, "Driver %s resumed", drv->name);
        drv->state = DRIVER_RUNNING;
    }
    else
    {
        ESP_LOGE(TAG, "Error resuming driver %s: %d (%s)", drv->name, res, esp_err_to_name(res));
        drv->state = DRIVER_INVALID;
    }

    return res;
}

esp_err_t driver_stop(driver_t *drv)
{
    CHECK_ARG(drv);

    if (drv->state != DRIVER_RUNNING && drv->state != DRIVER_SUSPENDED && drv->state != DRIVER_INVALID)
    {
        ESP_LOGE(TAG, ERR_INVALID_STATE, drv->name);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t res = ESP_OK;
    if (drv->stop)
        res = drv->stop(drv);
    if (res == ESP_OK)
    {
        ESP_LOGI(TAG, "Driver %s stopped", drv->name);
        drv->state = DRIVER_FINISHED;
    }
    else
    {
        ESP_LOGE(TAG, "Error stopping driver %s: %d (%s)", drv->name, res, esp_err_to_name(res));
        drv->state = DRIVER_INVALID;
    }

    return res;
}

const char *driver_state_to_name(driver_state_t state)
{
    return state_names[state];
}

int config_get_int(cJSON *item, int def)
{
    return cJSON_IsNumber(item) ? (int)cJSON_GetNumberValue(item) : def;
}

float config_get_float(cJSON *item, float def)
{
    return cJSON_IsNumber(item) ? (float)cJSON_GetNumberValue(item) : def;
}

gpio_num_t config_get_gpio(cJSON *item, gpio_num_t def)
{
    int res = config_get_int(item, def);
    return res >= GPIO_NUM_MAX ? def : res;
}
