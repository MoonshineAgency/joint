#include "node.h"
#include <i2cdev.h>
#include "common.h"
#include "settings.h"
#include "system.h"
#include "cvector.h"
#include "driver.h"

#include "drivers/drv_aht.h"
#include "drivers/drv_ds18x20.h"

static bool initialized = false;

typedef struct
{
    driver_t *drv;
    const char *cfg;
} drv_descr_t;

static drv_descr_t drivers[] = {
    { .drv = &drv_aht, .cfg = "{ \"stack_size\": 4096, \"sensors\": [{ \"port\": 1, \"sda\": 13, \"scl\": 14, \"type\": 0 }] }" },
    { .drv = &drv_ds18x20, .cfg = "{ \"stack_size\": 4096, \"gpio\": 15, \"scan_interval\": 10 }" },
};
static const size_t driver_count = sizeof(drivers) / sizeof(drv_descr_t);

esp_err_t node_init()
{
    ESP_LOGI(TAG, "Initializing node %s...", settings.node.name);
    system_set_mode(MODE_OFFLINE);

    SYSTEM_CHECK(i2cdev_init());

    for (size_t i = 0; i < driver_count; i++)
        driver_init(drivers[i].drv, drivers[i].cfg);

    return ESP_OK;
}

esp_err_t node_online()
{
    if (system_mode() == MODE_ONLINE)
        return ESP_OK;

    ESP_LOGI(TAG, "Node goes online...");
    system_set_mode(MODE_ONLINE);

    if (!initialized)
    {
        for (size_t i = 0; i < driver_count; i++)
        {
            esp_err_t r = driver_start(drivers[i].drv);
            if (r != ESP_OK)
                ESP_LOGW(TAG, "Error starting driver %s: %d (%s)", drivers[i].drv->name, r, esp_err_to_name(r));
        }
        initialized = true;
    }
    else
    {
        for (size_t i = 0; i < driver_count; i++)
        {
            esp_err_t r = driver_resume(drivers[i].drv);
            if (r != ESP_OK)
                ESP_LOGW(TAG, "Error resuming driver %s: %d (%s)", drivers[i].drv->name, r, esp_err_to_name(r));
        }
    }

    return ESP_OK;
}

esp_err_t node_offline()
{
    if (system_mode() == MODE_OFFLINE)
        return ESP_OK;

    ESP_LOGI(TAG, "Node goes offline...");
    system_set_mode(MODE_OFFLINE);

    for (size_t i = 0; i < driver_count; i++)
    {
        esp_err_t r = driver_suspend(drivers[i].drv);
        if (r != ESP_OK)
            ESP_LOGW(TAG, "Error suspending driver %s: %d (%s)", drivers[i].drv->name, r, esp_err_to_name(r));
    }

    return ESP_OK;
}
