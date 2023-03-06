#include "api.h"
#include "common.h"
#include <esp_ota_ops.h>
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

static esp_err_t get_settings(httpd_req_t *req)
{
    cJSON *res = NULL;
    settings_to_json(&res);
    return respond_json(req, res);
}

static const httpd_uri_t route_get_settings = {
    .uri = "/api/settings",
    .method = HTTP_GET,
    .handler = get_settings,
    .user_ctx = NULL
};

////////////////////////////////////////////////////////////////////////////////

static esp_err_t post_settings(httpd_req_t *req)
{
    const char *msg = NULL;
    cJSON *json = NULL;
    esp_err_t err = ESP_OK;

    err = parse_post_json(req, &msg, &json);
    if (err != ESP_OK)
        goto exit;

    char buf[128] = { 0 };
    msg = buf;
    err = settings_from_json(json, buf);

exit:
    if (json) cJSON_Delete(json);
    return respond_api(req, err, msg);
}

static const httpd_uri_t route_post_settings = {
    .uri = "/api/settings",
    .method = HTTP_POST,
    .handler = post_settings,
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
    CHECK(httpd_register_uri_handler(server, &route_get_settings));
    CHECK(httpd_register_uri_handler(server, &route_post_settings));
    CHECK(httpd_register_uri_handler(server, &route_get_reboot));

    return ESP_OK;
}
