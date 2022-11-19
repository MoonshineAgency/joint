#include "api.h"
#include "common.h"
#include <lwip/ip_addr.h>
#include <esp_ota_ops.h>
#include <driver/gpio.h>
#include <cJSON.h>
#include "settings.h"

static esp_err_t respond_json(httpd_req_t *req, cJSON *resp)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    char *txt = cJSON_Print(resp);
    esp_err_t res = httpd_resp_sendstr(req, txt);
    free(txt);

    cJSON_Delete(resp);

    return res;
}

static esp_err_t respond_api(httpd_req_t *req, esp_err_t err, const char *message)
{
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "result", err);
    cJSON_AddStringToObject(resp, "name", esp_err_to_name(err));
    cJSON_AddStringToObject(resp, "message", message ? message : "");

    return respond_json(req, resp);
}

static esp_err_t parse_post_json(httpd_req_t *req, const char **msg, cJSON **json)
{
    *msg = NULL;
    *json = NULL;
    esp_err_t err = ESP_OK;
    int res;
    char *buf = NULL;

    if (!req->content_len)
    {
        err = ESP_ERR_INVALID_ARG;
        *msg = "No POST data";
        goto exit;
    }

    if (req->content_len >= MAX_POST_SIZE)
    {
        err = ESP_ERR_NO_MEM;
        *msg = "POST data too big";
        goto exit;
    }

    buf = malloc(req->content_len + 1);
    if (!buf)
    {
        err = ESP_ERR_NO_MEM;
        *msg = "Out of memory";
        goto exit;
    }

    res = httpd_req_recv(req, buf, req->content_len);
    if (res != req->content_len)
    {
        err = ESP_FAIL;
        *msg = res ? "Socket error" : "Connection closed by peer";
        goto exit;
    }
    buf[req->content_len] = 0;

    *json = cJSON_Parse(buf);
    if (!*json)
    {
        err = ESP_ERR_INVALID_ARG;
        *msg = "Invalid JSON";
    }

exit:
    if (buf)
        free(buf);
    return err;
}

////////////////////////////////////////////////////////////////////////////////

static esp_err_t get_info(httpd_req_t *req)
{
    const esp_app_desc_t *app_desc = esp_app_get_description();

    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "app_name", app_desc->project_name);
    cJSON_AddStringToObject(res, "app_version", app_desc->version);
    cJSON_AddStringToObject(res, "build_date", app_desc->date);
    cJSON_AddStringToObject(res, "idf_ver", app_desc->idf_ver);

    return respond_json(req, res);
}

static const httpd_uri_t route_get_info = {
    .uri = "/api/info",
    .method = HTTP_GET,
    .handler = get_info,
    .user_ctx = NULL
};

////////////////////////////////////////////////////////////////////////////////

static esp_err_t get_settings_reset(httpd_req_t *req)
{
    esp_err_t res = settings_reset();
    return respond_api(req, res, res == ESP_OK ? "Reboot to apply" : NULL);
}

static const httpd_uri_t route_get_settings_reset = {
    .uri = "/api/settings/reset",
    .method = HTTP_GET,
    .handler = get_settings_reset,
    .user_ctx = NULL
};

////////////////////////////////////////////////////////////////////////////////

static esp_err_t get_settings_wifi(httpd_req_t *req)
{
    cJSON *res = cJSON_CreateObject();

    cJSON *ip = cJSON_CreateObject();
    cJSON_AddItemToObject(res, "ip", ip);
    cJSON_AddBoolToObject(ip, "dhcp", settings.wifi.ip.dhcp);
    cJSON_AddStringToObject(ip, "ip", settings.wifi.ip.ip);
    cJSON_AddStringToObject(ip, "netmask", settings.wifi.ip.netmask);
    cJSON_AddStringToObject(ip, "gateway", settings.wifi.ip.gateway);
    cJSON_AddStringToObject(ip, "dns", settings.wifi.ip.dns);

    cJSON *ap = cJSON_CreateObject();
    cJSON_AddItemToObject(res, "ap", ap);
    cJSON_AddStringToObject(ap, "ssid", (char *)settings.wifi.ap.ssid);
    cJSON_AddNumberToObject(ap, "channel", settings.wifi.ap.channel);
    cJSON_AddStringToObject(ap, "password", (char *)settings.wifi.ap.password);

    cJSON *sta = cJSON_CreateObject();
    cJSON_AddItemToObject(res, "sta", sta);
    cJSON_AddStringToObject(sta, "ssid", (char *)settings.wifi.sta.ssid);
    cJSON_AddStringToObject(sta, "password", (char *)settings.wifi.sta.password);

    return respond_json(req, res);
}

static const httpd_uri_t route_get_settings_wifi = {
    .uri = "/api/settings/wifi",
    .method = HTTP_GET,
    .handler = get_settings_wifi,
    .user_ctx = NULL
};

////////////////////////////////////////////////////////////////////////////////

static esp_err_t post_settings_wifi(httpd_req_t *req)
{
    const char *msg = NULL;
    cJSON *json = NULL;
    esp_err_t err = ESP_OK;

    {
        err = parse_post_json(req, &msg, &json);
        if (err != ESP_OK)
            goto exit;

        cJSON *ip = cJSON_GetObjectItem(json, "ip");
        if (!ip)
        {
            msg = "Object `ip` not found or invalid";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }
        cJSON *ip_dhcp_item = cJSON_GetObjectItem(ip, "dhcp");
        if (!cJSON_IsBool(ip_dhcp_item))
        {
            msg = "Object `ip.dhcp` not found or invalid";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }
        cJSON *ip_ip_item = cJSON_GetObjectItem(ip, "ip");
        if (!cJSON_IsString(ip_ip_item) || ipaddr_addr(cJSON_GetStringValue(ip_ip_item)) == IPADDR_NONE)
        {
            msg = "Item `ip.ip` not found, invalid or not an IP address";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }
        cJSON *ip_netmask_item = cJSON_GetObjectItem(ip, "netmask");
        if (!cJSON_IsString(ip_netmask_item) || ipaddr_addr(cJSON_GetStringValue(ip_netmask_item)) == IPADDR_NONE)
        {
            msg = "Item `ip.netmask` not found, invalid or not an IP address";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }
        cJSON *ip_gateway_item = cJSON_GetObjectItem(ip, "gateway");
        if (!cJSON_IsString(ip_gateway_item) || ipaddr_addr(cJSON_GetStringValue(ip_gateway_item)) == IPADDR_NONE)
        {
            msg = "Item `ip.gateway` not found, invalid or not an IP address";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }
        cJSON *ip_dns_item = cJSON_GetObjectItem(ip, "dns");
        if (!cJSON_IsString(ip_dns_item) || ipaddr_addr(cJSON_GetStringValue(ip_dns_item)) == IPADDR_NONE)
        {
            msg = "Item `ip.dns` not found, invalid or not an IP address";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }

        cJSON *ap = cJSON_GetObjectItem(json, "ap");
        if (!ap)
        {
            msg = "Object `ap` not found or invalid";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }
        cJSON *ap_ssid_item = cJSON_GetObjectItem(ap, "ssid");
        if (!cJSON_IsString(ap_ssid_item))
        {
            msg = "Item `ap.ssid` not found or invalid";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }
        cJSON *ap_channel_item = cJSON_GetObjectItem(ap, "channel");
        if (!cJSON_IsNumber(ap_channel_item))
        {
            msg = "Item `ap.channel` not found or invalid";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }
        cJSON *ap_password_item = cJSON_GetObjectItem(ap, "password");
        if (!cJSON_IsString(ap_password_item))
        {
            msg = "Item `ap.password` not found or invalid";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }

        cJSON *sta = cJSON_GetObjectItem(json, "sta");
        if (!sta)
        {
            msg = "Object `sta` not found or invalid";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }
        cJSON *sta_ssid_item = cJSON_GetObjectItem(sta, "ssid");
        if (!cJSON_IsString(sta_ssid_item))
        {
            msg = "Item `sta.ssid` not found or invalid";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }
        cJSON *sta_password_item = cJSON_GetObjectItem(sta, "password");
        if (!cJSON_IsString(sta_password_item))
        {
            msg = "Item `sta.password` not found or invalid";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }

        const char *ap_ssid = cJSON_GetStringValue(ap_ssid_item);
        size_t len = strlen(ap_ssid);
        if (!len || len >= sizeof(settings.wifi.ap.ssid))
        {
            msg = "Invalid ap.ssid";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }
        uint8_t ap_channel = (uint8_t)cJSON_GetNumberValue(ap_channel_item);
        if (ap_channel > 32)
        {
            msg = "Invalid ap.channel";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }
        const char *ap_password = cJSON_GetStringValue(ap_password_item);
        len = strlen(ap_password);
        if (len >= sizeof(settings.wifi.ap.password))
        {
            msg = "ap.password too long";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }
        const char *sta_ssid = cJSON_GetStringValue(sta_ssid_item);
        len = strlen(sta_ssid);
        if (!len || len >= sizeof(settings.wifi.sta.ssid))
        {
            msg = "Invalid sta.ssid";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }
        const char *sta_password = cJSON_GetStringValue(sta_password_item);
        len = strlen(sta_password);
        if (len >= sizeof(settings.wifi.sta.password))
        {
            msg = "sta.password too long";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }

        settings.wifi.ip.dhcp = cJSON_IsTrue(ip_dhcp_item);
        strncpy(settings.wifi.ip.ip, cJSON_GetStringValue(ip_ip_item), sizeof(settings.wifi.ip.ip) - 1);
        strncpy(settings.wifi.ip.netmask, cJSON_GetStringValue(ip_netmask_item), sizeof(settings.wifi.ip.netmask) - 1);
        strncpy(settings.wifi.ip.gateway, cJSON_GetStringValue(ip_gateway_item), sizeof(settings.wifi.ip.gateway) - 1);
        strncpy(settings.wifi.ip.dns, cJSON_GetStringValue(ip_dns_item), sizeof(settings.wifi.ip.dns) - 1);
        settings.wifi.ap.channel = ap_channel;
        memset(settings.wifi.ap.ssid, 0, sizeof(settings.wifi.ap.ssid));
        memset(settings.wifi.ap.password, 0, sizeof(settings.wifi.ap.password));
        memset(settings.wifi.sta.ssid, 0, sizeof(settings.wifi.sta.ssid));
        memset(settings.wifi.sta.password, 0, sizeof(settings.wifi.sta.password));
        strncpy((char *) settings.wifi.ap.ssid, ap_ssid, sizeof(settings.wifi.ap.ssid) - 1);
        strncpy((char *) settings.wifi.ap.password, ap_password, sizeof(settings.wifi.ap.password) - 1);
        strncpy((char *) settings.wifi.sta.ssid, sta_ssid, sizeof(settings.wifi.sta.ssid) - 1);
        strncpy((char *) settings.wifi.sta.password, sta_password, sizeof(settings.wifi.sta.password) - 1);

        err = settings_save();
        msg = err != ESP_OK
              ? "Error saving wifi settings"
              : "Settings saved, reboot to apply";
    }

exit:
    if (json) cJSON_Delete(json);
    return respond_api(req, err, msg);
}

static const httpd_uri_t route_post_settings_wifi = {
    .uri = "/api/settings/wifi",
    .method = HTTP_POST,
    .handler = post_settings_wifi,
    .user_ctx = NULL
};

////////////////////////////////////////////////////////////////////////////////

static esp_err_t get_settings_mqtt(httpd_req_t *req)
{
    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "uri", settings.mqtt.uri);
    cJSON_AddStringToObject(res, "username", settings.mqtt.username);
    cJSON_AddStringToObject(res, "password", settings.mqtt.password);

    return respond_json(req, res);
}

static const httpd_uri_t route_get_settings_mqtt = {
    .uri = "/api/settings/mqtt",
    .method = HTTP_GET,
    .handler = get_settings_mqtt,
    .user_ctx = NULL
};

////////////////////////////////////////////////////////////////////////////////

static esp_err_t post_settings_mqtt(httpd_req_t *req)
{
    const char *msg = NULL;
    cJSON *json = NULL;
    esp_err_t err = ESP_OK;

    {
        err = parse_post_json(req, &msg, &json);
        if (err != ESP_OK)
            goto exit;

        cJSON *uri_item = cJSON_GetObjectItem(json, "uri");
        if (!cJSON_IsString(uri_item))
        {
            msg = "Item `uri` not found or invalid";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }
        cJSON *username_item = cJSON_GetObjectItem(json, "username");
        if (!cJSON_IsString(username_item))
        {
            msg = "Item `username` not found or invalid";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }
        cJSON *password_item = cJSON_GetObjectItem(json, "password");
        if (!cJSON_IsString(password_item))
        {
            msg = "Item `password` not found or invalid";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }

        strncpy(settings.mqtt.uri, cJSON_GetStringValue(uri_item), sizeof(settings.mqtt.uri) - 1);
        settings.mqtt.uri[sizeof(settings.mqtt.uri) - 1] = '\0';
        strncpy(settings.mqtt.username, cJSON_GetStringValue(username_item), sizeof(settings.mqtt.username) - 1);
        settings.mqtt.username[sizeof(settings.mqtt.username) - 1] = '\0';
        strncpy(settings.mqtt.password, cJSON_GetStringValue(password_item), sizeof(settings.mqtt.password) - 1);
        settings.mqtt.password[sizeof(settings.mqtt.password) - 1] = '\0';

        err = settings_save();
        msg = err != ESP_OK
              ? "Error saving mqtt settings"
              : "Settings saved, reboot to apply";
    }

exit:
    if (json) cJSON_Delete(json);
    return respond_api(req, err, msg);
}

static const httpd_uri_t route_post_settings_mqtt = {
    .uri = "/api/settings/mqtt",
    .method = HTTP_POST,
    .handler = post_settings_mqtt,
    .user_ctx = NULL
};

////////////////////////////////////////////////////////////////////////////////

static esp_err_t get_settings_system(httpd_req_t *req)
{
    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "name", settings.node.name);
    cJSON_AddBoolToObject(res, "failsafe", settings.system.failsafe);
    cJSON_AddBoolToObject(res, "safe_mode", settings.system.safe_mode);

    return respond_json(req, res);
}

static const httpd_uri_t route_get_settings_system = {
    .uri = "/api/settings/system",
    .method = HTTP_GET,
    .handler = get_settings_system,
    .user_ctx = NULL
};

////////////////////////////////////////////////////////////////////////////////

static esp_err_t post_settings_system(httpd_req_t *req)
{
    const char *msg = NULL;
    cJSON *json = NULL;
    esp_err_t err = ESP_OK;

    {
        err = parse_post_json(req, &msg, &json);
        if (err != ESP_OK)
            goto exit;

        cJSON *name_item = cJSON_GetObjectItem(json, "name");
        if (!cJSON_IsString(name_item))
        {
            msg = "Item `name` not found or invalid";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }
        cJSON *failsafe_item = cJSON_GetObjectItem(json, "failsafe");
        if (!cJSON_IsBool(failsafe_item))
        {
            msg = "Item `failsafe` not found or invalid";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }
        cJSON *safe_mode_item = cJSON_GetObjectItem(json, "safe_mode");
        if (!cJSON_IsBool(safe_mode_item))
        {
            msg = "Item `safe_mode` not found or invalid";
            err = ESP_ERR_INVALID_ARG;
            goto exit;
        }

        strncpy(settings.node.name, cJSON_GetStringValue(name_item), sizeof(settings.node.name) - 1);
        settings.node.name[sizeof(settings.node.name) - 1] = '\0';
        settings.system.failsafe = cJSON_IsTrue(failsafe_item);
        settings.system.safe_mode = cJSON_IsTrue(safe_mode_item);

        err = settings_save();
        msg = err != ESP_OK
              ? "Error saving system settings"
              : "Settings saved, reboot to apply";
    }

exit:
    if (json) cJSON_Delete(json);
    return respond_api(req, err, msg);
}

static const httpd_uri_t route_post_settings_system = {
    .uri = "/api/settings/system",
    .method = HTTP_POST,
    .handler = post_settings_system,
    .user_ctx = NULL
};

////////////////////////////////////////////////////////////////////////////////

static esp_err_t get_reboot(httpd_req_t *req)
{
    (void)req;
    esp_restart();

    return ESP_OK; // dummy
}

static const httpd_uri_t route_get_reboot = {
    .uri = "/api/reboot",
    .method = HTTP_GET,
    .handler = get_reboot,
    .user_ctx = NULL
};

////////////////////////////////////////////////////////////////////////////////

esp_err_t api_init(httpd_handle_t server)
{
    CHECK(httpd_register_uri_handler(server, &route_get_info));
    CHECK(httpd_register_uri_handler(server, &route_get_settings_reset));
    CHECK(httpd_register_uri_handler(server, &route_get_settings_wifi));
    CHECK(httpd_register_uri_handler(server, &route_post_settings_wifi));
    CHECK(httpd_register_uri_handler(server, &route_get_settings_mqtt));
    CHECK(httpd_register_uri_handler(server, &route_post_settings_mqtt));
    CHECK(httpd_register_uri_handler(server, &route_get_settings_system));
    CHECK(httpd_register_uri_handler(server, &route_post_settings_system));
    CHECK(httpd_register_uri_handler(server, &route_get_reboot));

    return ESP_OK;
}
