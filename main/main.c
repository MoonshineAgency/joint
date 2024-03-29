#include <i2cdev.h>
#include "common.h"
#include "system_clock.h"
#include "system.h"
#include "settings.h"
#include "bus.h"
#include "reset_button.h"
#include "safe_mode.h"
#include "normal_mode.h"

void app_main()
{
    ESP_ERROR_CHECK(i2cdev_init());
    // Init basic system
    ESP_ERROR_CHECK(system_init());

    // Init settings storage, reset if storage is not formatted
    ESP_ERROR_CHECK(settings_init());
    // Load settings
    ESP_ERROR_CHECK(settings_load());
    // Init system clock
    ESP_ERROR_CHECK(system_clock_init());

    // Init system bus
    ESP_ERROR_CHECK(bus_init());
    ESP_ERROR_CHECK(reset_button_init());
    // Start device
    if (settings.system.safe_mode)
        ESP_ERROR_CHECK(safe_mode_start());
    else
        ESP_ERROR_CHECK(normal_mode_start());
}
