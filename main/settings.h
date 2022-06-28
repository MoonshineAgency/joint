#ifndef ESP_IOT_NODE_PLUS_SETTINGS_H_
#define ESP_IOT_NODE_PLUS_SETTINGS_H_

#include <esp_err.h>
#include <stdbool.h>
#include <esp_wifi.h>

typedef struct
{
    struct {
        bool safe_mode;
        bool failsafe;
    } system;

    struct {
        char name[32];
    } node;

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

#endif // ESP_IOT_NODE_PLUS_SETTINGS_H_
