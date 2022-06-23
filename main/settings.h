#ifndef ESP_IOT_NODE_PLUS_SETTINGS_H_
#define ESP_IOT_NODE_PLUS_SETTINGS_H_

#include "common.h"
#include "basic_settings.h"

namespace settings {

struct data_t
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
};

class settings_t : public basic_settings_t<data_t>
{
protected:
    esp_err_t _on_reset() override;

public:
    settings_t(const char *name, uint32_t magic, const data_t *defaults)
        : basic_settings_t<data_t>(name, magic, defaults)
    {}
};

void create(const char *name, uint32_t magic);
extern settings_t *get();

} // namespace settings

#endif // ESP_IOT_NODE_PLUS_SETTINGS_H_
