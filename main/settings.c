#include "settings.h"
#include "system.h"
#include <string.h>
#include <nvs_flash.h>
#include <nvs.h>

static const char *OPT_MAGIC    = "magic";
static const char *OPT_SETTINGS = "settings";

settings_t settings = { 0 };

static settings_t defaults = {
    .system = {
        .safe_mode = true,
        .failsafe = true,
    },
    .node = {
        .name = "",
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
            .channel        = CONFIG_NODE_WIFI_AP_CHANNEL,
            .password       = CONFIG_NODE_WIFI_AP_PASSWD,
            .max_connection = DEFAULT_WIFI_AP_MAXCONN,
            .authmode       = WIFI_AUTH_WPA_WPA2_PSK
        },
        .sta = {
            .ssid               = CONFIG_NODE_WIFI_STA_SSID,
            .password           = CONFIG_NODE_WIFI_STA_PASSWD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    },
};

static esp_err_t storage_load(const char *storage_name, void *target, size_t size)
{
    nvs_handle_t nvs;
    esp_err_t res;

    ESP_LOGD(TAG, "Reading settings from '%s'...", storage_name);

    CHECK(nvs_open(storage_name, NVS_READONLY, &nvs));

    uint32_t magic = 0;
    res = nvs_get_u32(nvs, OPT_MAGIC, &magic);
    if (magic != SETTINGS_MAGIC)
    {
        ESP_LOGW(TAG, "Invalid magic 0x%08x, expected 0x%08x", magic, SETTINGS_MAGIC);
        res = ESP_FAIL;
    }
    if (res != ESP_OK)
    {
        nvs_close(nvs);
        return res;
    }

    size_t tmp = size;
    res = nvs_get_blob(nvs, OPT_SETTINGS, target, &tmp);
    if (tmp != size)
    {
        ESP_LOGW(TAG, "Invalid settings size");
        res = ESP_FAIL;
    }
    if (res != ESP_OK)
    {
        ESP_LOGE(TAG, "Error reading settings %d (%s)", res, esp_err_to_name(res));
        nvs_close(nvs);
        return res;
    }

    nvs_close(nvs);

    ESP_LOGD(TAG, "Settings read from '%s'", storage_name);

    return ESP_OK;
}

static esp_err_t storage_save(const char *storage_name, const void *source, size_t size)
{
    ESP_LOGD(TAG, "Saving settings to '%s'...", storage_name);

    nvs_handle_t nvs;
    CHECK_LOGE(nvs_open(storage_name, NVS_READWRITE, &nvs),
            "Could not open NVS to write");
    CHECK_LOGE(nvs_set_u32(nvs, OPT_MAGIC, SETTINGS_MAGIC),
            "Error writing NVS magic");
    CHECK_LOGE(nvs_set_blob(nvs, OPT_SETTINGS, source, size),
            "Error writing NVS settings");
    nvs_close(nvs);

    ESP_LOGD(TAG, "Settings saved to '%s'", storage_name);

    return ESP_OK;
}

////////////////////////////////////////////////////////////////////////////////

esp_err_t settings_init()
{
    ESP_LOGW(TAG, "Initializing NVS storage...");
    esp_err_t res = nvs_flash_init();
    if (res == ESP_ERR_NVS_NO_FREE_PAGES || res == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "Formatting NVS storage...");
        CHECK(nvs_flash_erase());
        CHECK(nvs_flash_init());
        return settings_reset();
    }
    return res;
}

esp_err_t settings_reset()
{
    strncpy(defaults.node.name, SYSTEM_ID, sizeof(defaults.node.name) - 1);
    ESP_LOGW(TAG, "Resetting settings to defaults...");
    memcpy(&settings, &defaults, sizeof(settings_t));
    // override safe_mode
    settings.system.safe_mode = true;
    //settings.system.safe_mode = false;
    CHECK(settings_save());

    return ESP_OK;
}

esp_err_t settings_load()
{
    ESP_LOGI(TAG, "Loading settings...");
    esp_err_t res = storage_load(OPT_SETTINGS, &settings, sizeof(settings_t));
    if (res != ESP_OK)
        return settings_reset();

    return ESP_OK;
}

esp_err_t settings_save()
{
    ESP_LOGI(TAG, "Saving settings...");
    return storage_save(OPT_SETTINGS, &settings, sizeof(settings_t));
}
