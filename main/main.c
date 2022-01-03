#include "system.h"
#include "settings.h"
#include "safemode.h"
#include "node.h"
#include "bus.h"

void app_main()
{
    // Init basic system
    ESP_ERROR_CHECK(system_init());
    // Init settings storage, reset if storage is unformatted
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
