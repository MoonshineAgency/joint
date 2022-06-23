#include "settings.h"
#include <cstring>
#include <nvs.h>

namespace settings {

static settings_t *instance = nullptr;

const data_t defaults = {
    .system = {
        .safe_mode = true,
        .failsafe = true,
    },
    .node = {
        .name = { 0 },
    },
    .mqtt = {
        {.uri = CONFIG_NODE_MQTT_URI},
        {.username = CONFIG_NODE_MQTT_USERNAME},
        {.password = CONFIG_NODE_MQTT_PASSWORD},
    },
    .wifi = {
        .ip = {
            .dhcp    = DEFAULT_WIFI_DHCP,
            {.ip      = CONFIG_NODE_WIFI_IP},
            {.netmask = CONFIG_NODE_WIFI_NETMASK},
            {.gateway = CONFIG_NODE_WIFI_GATEWAY},
            {.dns     = CONFIG_NODE_WIFI_DNS},
        },
        .ap = {
            {.ssid           = CONFIG_NODE_WIFI_AP_SSID},
            {.password       = CONFIG_NODE_WIFI_AP_PASSWD},
            .channel        = CONFIG_NODE_WIFI_AP_CHANNEL,
            .authmode       = WIFI_AUTH_WPA_WPA2_PSK,
            .max_connection = DEFAULT_WIFI_AP_MAXCONN,
        },
        .sta = {
            {.ssid               = CONFIG_NODE_WIFI_STA_SSID},
            {.password           = CONFIG_NODE_WIFI_STA_PASSWD},
            .threshold = {
                .authmode = WIFI_AUTH_WPA2_PSK,
            },
        },
    },
};

void create(const char *name, uint32_t magic)
{
    instance = new settings_t(name, magic, &defaults);
}

extern settings_t *get()
{
    return instance;
}

esp_err_t settings_t::_on_reset()
{
    // override safe_mode
    data.system.safe_mode = true;
    // set default node name
    std::memset(data.node.name, 0, sizeof(data.node.name));
    std::strncpy(data.node.name, SYSTEM_ID, sizeof(data.node.name) - 1);

    return ESP_OK;
}

} // namespace settings
