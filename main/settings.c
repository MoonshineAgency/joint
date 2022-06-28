#include "settings.h"
#include "common.h"
#include <nvs.h>
#include <nvs_flash.h>

settings_t settings;

const settings_t defaults = {
    .system = {
        .safe_mode = true,
        .failsafe = true,
    },
    .node = {
        .name = { 0 },
    },
    .mqtt = {
        .uri = CONFIG_NODE_MQTT_URI,
        .username = CONFIG_NODE_MQTT_USERNAME,
        .password = CONFIG_NODE_MQTT_PASSWORD,
    },
    .wifi = {
        .ip = {
            .dhcp    = DEFAULT_WIFI_DHCP,
            .ip      = CONFIG_NODE_WIFI_IP,
            .netmask = CONFIG_NODE_WIFI_NETMASK,
            .gateway = CONFIG_NODE_WIFI_GATEWAY,
            .dns     = CONFIG_NODE_WIFI_DNS,
        },
        .ap = {
            .ssid           = CONFIG_NODE_WIFI_AP_SSID,
            .password       = CONFIG_NODE_WIFI_AP_PASSWD,
            .channel        = CONFIG_NODE_WIFI_AP_CHANNEL,
            .authmode       = WIFI_AUTH_WPA_WPA2_PSK,
            .max_connection = DEFAULT_WIFI_AP_MAXCONN,
        },
        .sta = {
            .ssid               = CONFIG_NODE_WIFI_STA_SSID,
            .password           = CONFIG_NODE_WIFI_STA_PASSWD,
            .threshold = {
                .authmode = WIFI_AUTH_WPA2_PSK,
            },
        },
    },
};

esp_err_t settings_init()
{
    ESP_LOGI(TAG, "Initializing NVS storage...");
    esp_err_t res = nvs_flash_init();
    if (res == ESP_ERR_NVS_NO_FREE_PAGES || res == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "Formatting NVS storage...");
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "Error erasing flash");
        ESP_RETURN_ON_ERROR(nvs_flash_init(), TAG, "Error initializing flash");
        return settings_reset();
    }
    return res;
}

esp_err_t settings_load()
{
    ESP_LOGI(TAG, "Reading settings from '%s'...", SETTINGS_PARTITION);

    // Open NVS partition
    nvs_handle_t nvs;
    esp_err_t res = nvs_open(SETTINGS_PARTITION, NVS_READONLY, &nvs);
    if (res != ESP_OK && res != ESP_ERR_NVS_NOT_FOUND)
        ESP_RETURN_ON_ERROR(res, TAG, "Error opening NVS partition '%s'", SETTINGS_PARTITION);
    if (res == ESP_ERR_NVS_NOT_FOUND)
        goto exit;

    // Check magic value
    uint32_t magic = 0;
    res = nvs_get_u32(nvs, SETTINGS_MAGIC_KEY, &magic);
    if (res == ESP_OK && magic != SETTINGS_MAGIC_VAL)
    {
        ESP_LOGW(TAG, "Invalid magic 0x%08x, expected 0x%08x", magic, SETTINGS_MAGIC_VAL);
        res = ESP_FAIL;
    }
    if (res != ESP_OK)
        goto exit;

    // Read settings data
    size_t tmp = sizeof(settings_t);
    res = nvs_get_blob(nvs, SETTINGS_DATA_KEY, &settings, &tmp);
    if (tmp != sizeof(settings_t))
    {
        ESP_LOGW(TAG, "Invalid settings size");
        res = ESP_FAIL;
    }
    if (res != ESP_OK)
        ESP_LOGE(TAG, "Error reading settings %d (%s)", res, esp_err_to_name(res));

exit:
    nvs_close(nvs);
    return res == ESP_OK ? res : settings_reset();
}

esp_err_t settings_save()
{
    ESP_LOGI(TAG, "Saving settings to '%s'...", SETTINGS_PARTITION);

    nvs_handle_t nvs;
    ESP_RETURN_ON_ERROR(
        nvs_open(SETTINGS_PARTITION, NVS_READWRITE, &nvs),
        TAG, "Could not open NVS to write");
    ESP_RETURN_ON_ERROR(
        nvs_set_u32(nvs, SETTINGS_MAGIC_KEY, SETTINGS_MAGIC_VAL),
        TAG, "Error writing NVS magic");
    ESP_RETURN_ON_ERROR(
        nvs_set_blob(nvs, SETTINGS_DATA_KEY, &settings, sizeof(settings_t)),
        TAG, "Error writing NVS settings");
    nvs_close(nvs);

    return ESP_OK;
}

esp_err_t settings_reset()
{
    ESP_LOGW(TAG, "Resetting settings to defaults...");
    memcpy(&settings, &defaults, sizeof(settings_t));
    memset(settings.node.name, 0, sizeof(settings.node.name));
    strncpy(settings.node.name, SYSTEM_ID, sizeof(settings.node.name) - 1);
    return settings_save();
}
