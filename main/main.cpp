#include "common.h"
#include "system.h"
#include "settings.h"
#include "bus.h"
#include "safe_mode.h"
#include "normal_mode.h"

extern "C" void app_main()
{
    // Init basic system
    ESP_ERROR_CHECK(sys::init());
    // Create settings
    settings::create("settings", SETTINGS_MAGIC);
    // Init settings storage, reset if storage is not formatted
    ESP_ERROR_CHECK(settings::get()->init());
    // Load settings
    ESP_ERROR_CHECK(settings::get()->load());
    // Init system bus
    ESP_ERROR_CHECK(bus::init());
    // Start device
    if (settings::get()->data.system.safe_mode)
        ESP_ERROR_CHECK(safe_mode_start());
    else
        ESP_ERROR_CHECK(normal_mode_start());
}
