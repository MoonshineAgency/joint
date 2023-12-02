#ifndef ESP_IOT_NODE_PLUS_SETTINGS_H_
#define ESP_IOT_NODE_PLUS_SETTINGS_H_

#include <esp_err.h>
#include <stdbool.h>
#include <esp_wifi.h>
#include <cJSON.h>

typedef struct
{
    struct {
        bool safe_mode;
        bool failsafe;
        char name[32];
    } system;

    struct {
        bool enabled;
        char time_server[64];
        char tz[32];
    } sntp;

    struct {
        char uri[64];
        char username[32];
        char password[32];
    } mqtt;

    struct {
        struct {
            bool dhcp;
            char ip[16];
            char netmask[16];
            char gateway[16];
            char dns[16];
        } ip;
        wifi_ap_config_t ap;
        wifi_sta_config_t sta;
    } wifi;
} settings_t;

extern settings_t settings;

esp_err_t settings_init();
esp_err_t settings_load();
esp_err_t settings_save();
esp_err_t settings_reset();

esp_err_t settings_load_driver_config(const char *name, char *buf, size_t max_size);
esp_err_t settings_save_driver_config(const char *name, const char *config);
esp_err_t settings_reset_driver_config(const char *name);

esp_err_t settings_from_json(cJSON *src, char *msg);
esp_err_t settings_to_json(cJSON **tgt);

#endif // ESP_IOT_NODE_PLUS_SETTINGS_H_
