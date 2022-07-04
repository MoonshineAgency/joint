#include <i2cdev.h>
#include "common.h"
#include "hwrtc.h"
#include "system.h"
#include "settings.h"
#include "bus.h"
#include "safe_mode.h"
#include "normal_mode.h"

void app_main()
{
    ESP_ERROR_CHECK(i2cdev_init());
    // Init basic system
    ESP_ERROR_CHECK(system_init());

    esp_err_t r = hw_rtc_init();
    if (r != ESP_OK)
        ESP_LOGW(TAG, "Error initializing HW RTC: %d (%s)", r, esp_err_to_name(r));

    // Init settings storage, reset if storage is not formatted
    ESP_ERROR_CHECK(settings_init());
    // Load settings
    ESP_ERROR_CHECK(settings_load());
    // Init system bus
    ESP_ERROR_CHECK(bus_init());
    // Start device
    if (settings.system.safe_mode)
        ESP_ERROR_CHECK(safe_mode_start());
    else
        ESP_ERROR_CHECK(normal_mode_start());
}
