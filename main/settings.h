#ifndef MAIN_SETTINGS_H_
#define MAIN_SETTINGS_H_

#include "common.h"
#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

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

esp_err_t settings_reset();

esp_err_t settings_load();

esp_err_t settings_save();

#ifdef __cplusplus
}
#endif

#endif /* MAIN_SETTINGS_H_ */
