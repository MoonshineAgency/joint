#include "node.h"
#include <vector>
#include <i2cdev.h>

#include "settings.h"
#include "system.h"

#include "drivers/drv_aht.h"
#include "drivers/drv_ds18x20.h"

namespace node {

static std::vector<driver_t *> drivers;

esp_err_t init()
{
    ESP_LOGI(TAG, "Initializing node %s...", settings::get()->data.node.name);
    sys::set_mode(sys::MODE_OFFLINE);

    SYSTEM_CHECK(i2cdev_init());

    ///////////////////////////////////////////
    drivers.push_back(new aht_driver_t({
        {"stack_size", 2048},
        {"priority", tskIDLE_PRIORITY + 1},
        {"period", 1000},
        {"devices", 1},
        {"device0_type", AHT_TYPE_AHT1x},
        {"device0_scl", 14},
        {"device0_sda", 13},
    }));

    drivers.push_back(new ds18x20_driver_t({
        {"stack_size", 3072},
        {"priority", tskIDLE_PRIORITY + 1},
        {"period", 1000},
        {"rescan_interval", 10},
        {"gpio", 15},
    }));
    ///////////////////////////////////////////

    for (const auto &drv: drivers)
    {
        auto r = drv->init();
        if (r != ESP_OK)
        {
            ESP_LOGW(TAG, "Error initializing driver: %d (%s)", r, esp_err_to_name(r));
            continue;
        }

        r = drv->start();
        if (r != ESP_OK)
            ESP_LOGW(TAG, "Error starting driver': %d (%s)", r, esp_err_to_name(r));
    }

    return ESP_OK;
}

esp_err_t online()
{
    if (sys::mode() == sys::MODE_ONLINE)
        return ESP_OK;

    ESP_LOGI(TAG, "Node goes online...");
    sys::set_mode(sys::MODE_ONLINE);

    for (const auto &drv: drivers)
    {
        drv->resume();
    }

    return ESP_OK;
}

esp_err_t offline()
{
    if (sys::mode() == sys::MODE_OFFLINE)
        return ESP_OK;

    ESP_LOGI(TAG, "Node goes offline...");
    sys::set_mode(sys::MODE_OFFLINE);

    for (const auto &drv: drivers)
    {
        drv->suspend();
    }

    return ESP_OK;
}

} // namespace node
