#include "node.h"
#include "system.h"

esp_err_t node_init()
{
    ESP_LOGI(TAG, "Initializing node %s...", SYSTEM_ID);

    system_set_mode(MODE_OFFLINE);
    return ESP_OK;
}

esp_err_t node_online()
{
    ESP_LOGI(TAG, "Node goes online...");

    system_set_mode(MODE_ONLINE);
    return ESP_OK;
}

esp_err_t node_offline()
{
    ESP_LOGI(TAG, "Node goes offline...");

    system_set_mode(MODE_OFFLINE);
    return ESP_OK;
}
