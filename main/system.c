#include "system.h"
#include "common.h"
#include "bus.h"
#include <esp_ota_ops.h>
#include <esp_mac.h>

static const char *const mode_names[] = {
    [MODE_INIT]    = "INIT",
    [MODE_BOOT]    = "BOOT",
    [MODE_SAFE]    = "SAFE MODE",
    [MODE_OFFLINE] = "OFFLINE",
    [MODE_ONLINE]  = "ONLINE"
};

static mode_t cur_mode = MODE_INIT;
static mode_t prev_mode = MODE_INIT;

////////////////////////////////////////////////////////////////////////////////

const char *system_mode_name(mode_t val)
{
    return val < MODE_MAX ? mode_names[val] : NULL;
}

esp_err_t system_init()
{
    const esp_app_desc_t *app = esp_app_get_description();

    cur_mode = prev_mode = MODE_INIT;

    ESP_LOGI(TAG, "%s v.%s built %s", app->project_name, app->version, app->date);

    SYSTEM_ID = calloc(1, SYSTEM_ID_LEN);
    if (!SYSTEM_ID)
    {
        ESP_LOGE(TAG, "Out of memory at boot");
        return ESP_ERR_NO_MEM;
    }

    uint8_t mac[6] = { 0 };
    CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));

    snprintf((char *)SYSTEM_ID, SYSTEM_ID_LEN, CONFIG_NODE_SYS_ID,
              mac[0], mac[1], mac[3], mac[3], mac[4], mac[5]);

    ESP_LOGI(TAG, "System ID [%s]", SYSTEM_ID);

    return ESP_OK;
}

void system_set_mode(mode_t mode)
{
    if (mode == cur_mode)
        return;

    prev_mode = cur_mode;
    cur_mode = mode;

    ESP_LOGW(TAG, "Switching from mode %s to %s", system_mode_name(prev_mode), system_mode_name(cur_mode));

    CHECK_LOGW(bus_send_event(MODE_SET, NULL, 0),
               "Error sending event");
}

mode_t system_mode()
{
    return cur_mode;
}
