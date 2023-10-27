#include "settings.h"
#include "common.h"
#include <nvs.h>
#include <nvs_flash.h>
#include <lwip/ip_addr.h>

settings_t settings;

const settings_t defaults = {
    .system = {
        .safe_mode = true,
        .failsafe = true,
        .name = { 0 },
    },
    .sntp = {
        .enabled = false,
        .time_server = { 0 },
        .tz = DEFAULT_TZ,
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

static const char *OPT_SYSTEM            = "system";
static const char *OPT_SYSTEM_NAME       = "name";
static const char *OPT_SYSTEM_FAILSAFE   = "failsafe";
static const char *OPT_SYSTEM_SAFE_MODE  = "safe_mode";
static const char *OPT_SNTP              = "sntp";
static const char *OPT_SNTP_ENABLED      = "enabled";
static const char *OPT_SNTP_TIME_SERVER  = "timeserver";
static const char *OPT_SNTP_TZ           = "timezone";
static const char *OPT_MQTT              = "mqtt";
static const char *OPT_MQTT_URI          = "uri";
static const char *OPT_MQTT_USERNAME     = "username";
static const char *OPT_MQTT_PASSWORD     = "password";
static const char *OPT_WIFI              = "wifi";
static const char *OPT_WIFI_IP           = "ip";
static const char *OPT_WIFI_IP_DHCP      = "dhcp";
static const char *OPT_WIFI_IP_IP        = "ip";
static const char *OPT_WIFI_IP_NETMASK   = "netmask";
static const char *OPT_WIFI_IP_GATEWAY   = "gateway";
static const char *OPT_WIFI_IP_DNS       = "dns";
static const char *OPT_WIFI_AP           = "ap";
static const char *OPT_WIFI_AP_SSID      = "ssid";
static const char *OPT_WIFI_AP_CHANNEL   = "channel";
static const char *OPT_WIFI_AP_PASSWORD  = "password";
static const char *OPT_WIFI_STA          = "sta";
static const char *OPT_WIFI_STA_SSID     = "ssid";
static const char *OPT_WIFI_STA_PASSWORD = "password";

static const char *MSG_FIELD_ERR             = "Field `%s` not found or invalid";
static const char *MSG_SYSTEM_NAME_ERR       = "Invalid system.name";
static const char *MSG_SNTP_TZ_ERR           = "Invalid sntp.tz";
static const char *MSG_SNTP_TIME_SERVER_ERR  = "Invalid sntp.time_server";
static const char *MSG_MQTT_URI_ERR          = "Invalid mqtt.uri";
static const char *MSG_MQTT_USERNAME_ERR     = "mqtt.username too long";
static const char *MSG_MQTT_PASSWORD_ERR     = "mqtt.password too long";
static const char *MSG_WIFI_AP_SSID_ERR      = "Invalid ap.ssid";
static const char *MSG_WIFI_AP_CHANNEL_ERR   = "Invalid ap.channel";
static const char *MSG_WIFI_AP_PASSWORD_ERR  = "ap.password too long";
static const char *MSG_WIFI_STA_SSID_ERR     = "Invalid sta.ssid";
static const char *MSG_WIFI_STA_PASSWORD_ERR = "sta.password too long";
static const char *MSG_SAVE_ERR              = "Error saving settings";
static const char *MSG_SAVE_OK               = "Settings saved, reboot to apply";

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
        ESP_LOGW(TAG, "Invalid magic 0x%08" PRIx32 ", expected 0x%08x", magic, SETTINGS_MAGIC_VAL);
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
    memset(settings.system.name, 0, sizeof(settings.system.name));
    strncpy(settings.system.name, SYSTEM_ID, sizeof(settings.system.name) - 1);
    return settings_save();
}

esp_err_t settings_load_driver_config(const char *name, char *buf, size_t max_size)
{
    ESP_LOGI(TAG, "Loading '%s' driver configuration...", name);

    nvs_handle_t nvs;
    ESP_RETURN_ON_ERROR(
        nvs_open(SETTINGS_PARTITION, NVS_READONLY, &nvs),
        TAG, "Could not open NVS to read");
    size_t size;
    ESP_RETURN_ON_ERROR(
        nvs_get_str(nvs, name, NULL, &size),
        TAG, "Error reading '%s' driver configuration: %d (%s)", name, err_rc_, esp_err_to_name(err_rc_));
    if (size > max_size)
    {
        ESP_LOGE(TAG, "Configuration %s too big: %u", name, size);
        return ESP_ERR_NO_MEM;
    }
    esp_err_t r = nvs_get_str(nvs, name, buf, &size);
    nvs_close(nvs);

    return r;
}

esp_err_t settings_save_driver_config(const char *name, const char *config)
{
    ESP_LOGI(TAG, "Saving '%s' driver configuration...", name);
    nvs_handle_t nvs;
    ESP_RETURN_ON_ERROR(
        nvs_open(SETTINGS_PARTITION, NVS_READWRITE, &nvs),
        TAG, "Could not open NVS to write");
    ESP_RETURN_ON_ERROR(
        nvs_set_str(nvs, name, config),
        TAG, "Error writing '%s' driver configuration: %d (%s)", name, err_rc_, esp_err_to_name(err_rc_));
    nvs_close(nvs);

    return ESP_OK;
}

esp_err_t settings_reset_driver_config(const char *name)
{
    ESP_LOGI(TAG, "Erasing '%s' driver configuration...", name);
    nvs_handle_t nvs;
    ESP_RETURN_ON_ERROR(
        nvs_open(SETTINGS_PARTITION, NVS_READWRITE, &nvs),
        TAG, "Could not open NVS to write");
    ESP_RETURN_ON_ERROR(
        nvs_erase_key(nvs, name),
        TAG, "Error erasing '%s' driver configuration: %d (%s)", name, err_rc_, esp_err_to_name(err_rc_));
    nvs_close(nvs);

    return ESP_OK;
}

static inline bool is_ip(cJSON *json)
{
    return cJSON_IsString(json) && ipaddr_addr(cJSON_GetStringValue(json)) != IPADDR_NONE;
}

static void report_err(const char *err_msg, char *msg)
{
    snprintf(msg, SETTINGS_JSON_MSG_SIZE, "%s", err_msg);
    ESP_LOGE(TAG, "%s", msg);
}

static void report_json_field_err(const char *field, char *msg)
{
    snprintf(msg, SETTINGS_JSON_MSG_SIZE, MSG_FIELD_ERR, field);
    ESP_LOGE(TAG, "%s", msg);
}

#define GET_JSON_ITEM(VAR, SRC, NAME, CHECK) \
    cJSON *VAR = cJSON_GetObjectItem(SRC, NAME); \
    if (!CHECK(VAR)) { report_json_field_err(NAME, msg); return ESP_ERR_INVALID_ARG; }


esp_err_t settings_from_json(cJSON *src, char *msg)
{
    CHECK_ARG(src);

    // 1. Parse
    GET_JSON_ITEM(system, src, OPT_SYSTEM, );
    GET_JSON_ITEM(sys_name_item, system, OPT_SYSTEM_NAME, cJSON_IsString);
    GET_JSON_ITEM(sys_failsafe_item, system, OPT_SYSTEM_FAILSAFE, cJSON_IsBool);
    GET_JSON_ITEM(sys_safe_mode_item, system, OPT_SYSTEM_SAFE_MODE, cJSON_IsBool);

    GET_JSON_ITEM(sntp, src, OPT_SNTP, );
    GET_JSON_ITEM(sntp_enabled_item, sntp, OPT_SNTP_ENABLED, cJSON_IsBool);
    GET_JSON_ITEM(sntp_time_server_item, sntp, OPT_SNTP_TIME_SERVER, cJSON_IsString);
    GET_JSON_ITEM(sntp_tz_item, sntp, OPT_SNTP_TZ, cJSON_IsString);

    GET_JSON_ITEM(mqtt, src, OPT_MQTT, );
    GET_JSON_ITEM(mqtt_uri_item, mqtt, OPT_MQTT_URI, cJSON_IsString);
    GET_JSON_ITEM(mqtt_username_item, mqtt, OPT_MQTT_USERNAME, cJSON_IsString);
    GET_JSON_ITEM(mqtt_password_item, mqtt, OPT_MQTT_PASSWORD, cJSON_IsString);

    GET_JSON_ITEM(wifi, src, OPT_WIFI, );

    GET_JSON_ITEM(wifi_ip, wifi, OPT_WIFI_IP, );
    GET_JSON_ITEM(wifi_ip_dhcp_item, wifi_ip, OPT_WIFI_IP_DHCP, cJSON_IsBool);
    GET_JSON_ITEM(wifi_ip_ip_item, wifi_ip, OPT_WIFI_IP_IP, is_ip);
    GET_JSON_ITEM(wifi_ip_netmask_item, wifi_ip, OPT_WIFI_IP_NETMASK, is_ip);
    GET_JSON_ITEM(wifi_ip_gateway_item, wifi_ip, OPT_WIFI_IP_GATEWAY, is_ip);
    GET_JSON_ITEM(wifi_ip_dns_item, wifi_ip, OPT_WIFI_IP_DNS, is_ip);

    GET_JSON_ITEM(wifi_ap, wifi, OPT_WIFI_AP, );
    GET_JSON_ITEM(wifi_ap_ssid, wifi_ap, OPT_WIFI_AP_SSID, cJSON_IsString);
    GET_JSON_ITEM(wifi_ap_channel, wifi_ap, OPT_WIFI_AP_CHANNEL, cJSON_IsNumber);
    GET_JSON_ITEM(wifi_ap_password, wifi_ap, OPT_WIFI_AP_PASSWORD, cJSON_IsString);

    GET_JSON_ITEM(wifi_sta, wifi, OPT_WIFI_STA, );
    GET_JSON_ITEM(wifi_sta_ssid, wifi_sta, OPT_WIFI_STA_SSID, cJSON_IsString);
    GET_JSON_ITEM(wifi_sta_password, wifi_sta, OPT_WIFI_STA_PASSWORD, cJSON_IsString);

    // 2. Check
    const char *sys_name = cJSON_GetStringValue(sys_name_item);
    size_t len = strlen(sys_name);
    if (!len || len >= sizeof(settings.system.name))
    {
        report_err(MSG_SYSTEM_NAME_ERR, msg);
        return ESP_ERR_INVALID_ARG;
    }

    const char *sntp_time_server = cJSON_GetStringValue(sntp_time_server_item);
    len = strlen(sntp_time_server);
    if (!len || len >= sizeof(settings.sntp.time_server))
    {
        report_err(MSG_SNTP_TIME_SERVER_ERR, msg);
        return ESP_ERR_INVALID_ARG;
    }

    const char *sntp_tz = cJSON_GetStringValue(sntp_tz_item);
    len = strlen(sntp_tz);
    if (len >= sizeof(settings.sntp.tz))
    {
        report_err(MSG_SNTP_TZ_ERR, msg);
        return ESP_ERR_INVALID_ARG;
    }
    else if (!len)
        sntp_tz = DEFAULT_TZ;

    const char *mqtt_uri = cJSON_GetStringValue(mqtt_uri_item);
    len = strlen(mqtt_uri);
    if (!len || len >= sizeof(settings.mqtt.uri))
    {
        report_err(MSG_MQTT_URI_ERR, msg);
        return ESP_ERR_INVALID_ARG;
    }
    const char *mqtt_username = cJSON_GetStringValue(mqtt_username_item);
    if (strlen(mqtt_username) >= sizeof(settings.mqtt.username))
    {
        report_err(MSG_MQTT_USERNAME_ERR, msg);
        return ESP_ERR_INVALID_ARG;
    }
    const char *mqtt_password = cJSON_GetStringValue(mqtt_password_item);
    if (strlen(mqtt_password) >= sizeof(settings.mqtt.password))
    {
        report_err(MSG_MQTT_PASSWORD_ERR, msg);
        return ESP_ERR_INVALID_ARG;
    }

    const char *ap_ssid = cJSON_GetStringValue(wifi_ap_ssid);
    len = strlen(ap_ssid);
    if (!len || len >= sizeof(settings.wifi.ap.ssid))
    {
        report_err(MSG_WIFI_AP_SSID_ERR, msg);
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t ap_channel = (uint8_t)cJSON_GetNumberValue(wifi_ap_channel);
    if (ap_channel > 32)
    {
        report_err(MSG_WIFI_AP_CHANNEL_ERR, msg);
        return ESP_ERR_INVALID_ARG;
    }
    const char *ap_password = cJSON_GetStringValue(wifi_ap_password);
    if (strlen(ap_password) >= sizeof(settings.wifi.ap.password))
    {
        report_err(MSG_WIFI_AP_PASSWORD_ERR, msg);
        return ESP_ERR_INVALID_ARG;
    }
    const char *sta_ssid = cJSON_GetStringValue(wifi_sta_ssid);
    len = strlen(sta_ssid);
    if (!len || len >= sizeof(settings.wifi.sta.ssid))
    {
        report_err(MSG_WIFI_STA_SSID_ERR, msg);
        return ESP_ERR_INVALID_ARG;
    }
    const char *sta_password = cJSON_GetStringValue(wifi_sta_password);
    if (strlen(sta_password) >= sizeof(settings.wifi.sta.password))
    {
        report_err(MSG_WIFI_STA_PASSWORD_ERR, msg);
        return ESP_ERR_INVALID_ARG;
    }

    // 3. Update
    strncpy(settings.system.name, sys_name, sizeof(settings.system.name) - 1);
    settings.system.name[sizeof(settings.system.name) - 1] = '\0';
    settings.system.failsafe = cJSON_IsTrue(sys_failsafe_item);
    settings.system.safe_mode = cJSON_IsTrue(sys_safe_mode_item);

    strncpy(settings.sntp.time_server, sntp_time_server, sizeof(settings.sntp.tz) - 1);
    settings.sntp.time_server[sizeof(settings.sntp.time_server) - 1] = '\0';
    strncpy(settings.sntp.tz, sntp_tz, sizeof(settings.sntp.tz) - 1);
    settings.sntp.tz[sizeof(settings.sntp.tz) - 1] = '\0';

    strncpy(settings.mqtt.uri, mqtt_uri, sizeof(settings.mqtt.uri) - 1);
    settings.mqtt.uri[sizeof(settings.mqtt.uri) - 1] = '\0';
    strncpy(settings.mqtt.username, mqtt_username, sizeof(settings.mqtt.username) - 1);
    settings.mqtt.username[sizeof(settings.mqtt.username) - 1] = '\0';
    strncpy(settings.mqtt.password, mqtt_password, sizeof(settings.mqtt.password) - 1);
    settings.mqtt.password[sizeof(settings.mqtt.password) - 1] = '\0';

    settings.wifi.ip.dhcp = cJSON_IsTrue(wifi_ip_dhcp_item);
    strncpy(settings.wifi.ip.ip, cJSON_GetStringValue(wifi_ip_ip_item), sizeof(settings.wifi.ip.ip) - 1);
    strncpy(settings.wifi.ip.netmask, cJSON_GetStringValue(wifi_ip_netmask_item), sizeof(settings.wifi.ip.netmask) - 1);
    strncpy(settings.wifi.ip.gateway, cJSON_GetStringValue(wifi_ip_gateway_item), sizeof(settings.wifi.ip.gateway) - 1);
    strncpy(settings.wifi.ip.dns, cJSON_GetStringValue(wifi_ip_dns_item), sizeof(settings.wifi.ip.dns) - 1);
    settings.wifi.ap.channel = ap_channel;
    memset(settings.wifi.ap.ssid, 0, sizeof(settings.wifi.ap.ssid));
    memset(settings.wifi.ap.password, 0, sizeof(settings.wifi.ap.password));
    memset(settings.wifi.sta.ssid, 0, sizeof(settings.wifi.sta.ssid));
    memset(settings.wifi.sta.password, 0, sizeof(settings.wifi.sta.password));
    strncpy((char *) settings.wifi.ap.ssid, ap_ssid, sizeof(settings.wifi.ap.ssid) - 1);
    strncpy((char *) settings.wifi.ap.password, ap_password, sizeof(settings.wifi.ap.password) - 1);
    strncpy((char *) settings.wifi.sta.ssid, sta_ssid, sizeof(settings.wifi.sta.ssid) - 1);
    strncpy((char *) settings.wifi.sta.password, sta_password, sizeof(settings.wifi.sta.password) - 1);

    esp_err_t err = settings_save();
    report_err(err == ESP_OK ? MSG_SAVE_OK : MSG_SAVE_ERR, msg);

    return err;
}

esp_err_t settings_to_json(cJSON **tgt)
{
    CHECK_ARG(tgt);

    *tgt = cJSON_CreateObject();

    cJSON *system = cJSON_AddObjectToObject(*tgt, OPT_SYSTEM);
    cJSON_AddStringToObject(system, OPT_SYSTEM_NAME, settings.system.name);
    cJSON_AddBoolToObject(system, OPT_SYSTEM_FAILSAFE, settings.system.failsafe);
    cJSON_AddBoolToObject(system, OPT_SYSTEM_SAFE_MODE, settings.system.safe_mode);

    cJSON *sntp = cJSON_AddObjectToObject(*tgt, OPT_SNTP);
    cJSON_AddBoolToObject(sntp, OPT_SNTP_ENABLED, settings.sntp.enabled);
    cJSON_AddStringToObject(sntp, OPT_SNTP_TIME_SERVER, settings.sntp.time_server);
    cJSON_AddStringToObject(sntp, OPT_SNTP_TZ, settings.sntp.tz);

    cJSON *mqtt = cJSON_AddObjectToObject(*tgt, OPT_MQTT);
    cJSON_AddStringToObject(mqtt, OPT_MQTT_URI, settings.mqtt.uri);
    cJSON_AddStringToObject(mqtt, OPT_MQTT_USERNAME, settings.mqtt.username);
    cJSON_AddStringToObject(mqtt, OPT_MQTT_PASSWORD, settings.mqtt.password);

    cJSON *wifi = cJSON_AddObjectToObject(*tgt, OPT_WIFI);

    cJSON *ip = cJSON_AddObjectToObject(wifi, OPT_WIFI_IP);
    cJSON_AddBoolToObject(ip, OPT_WIFI_IP_DHCP, settings.wifi.ip.dhcp);
    cJSON_AddStringToObject(ip, OPT_WIFI_IP_IP, settings.wifi.ip.ip);
    cJSON_AddStringToObject(ip, OPT_WIFI_IP_NETMASK, settings.wifi.ip.netmask);
    cJSON_AddStringToObject(ip, OPT_WIFI_IP_GATEWAY, settings.wifi.ip.gateway);
    cJSON_AddStringToObject(ip, OPT_WIFI_IP_DNS, settings.wifi.ip.dns);

    cJSON *ap = cJSON_AddObjectToObject(wifi, OPT_WIFI_AP);
    cJSON_AddStringToObject(ap, OPT_WIFI_AP_SSID, (char *)settings.wifi.ap.ssid);
    cJSON_AddNumberToObject(ap, OPT_WIFI_AP_CHANNEL, settings.wifi.ap.channel);
    cJSON_AddStringToObject(ap, OPT_WIFI_AP_PASSWORD, (char *)settings.wifi.ap.password);

    cJSON *sta = cJSON_AddObjectToObject(wifi, OPT_WIFI_STA);
    cJSON_AddStringToObject(sta, OPT_WIFI_STA_SSID, (char *)settings.wifi.sta.ssid);
    cJSON_AddStringToObject(sta, OPT_WIFI_STA_PASSWORD, (char *)settings.wifi.sta.password);

    return ESP_OK;
}
