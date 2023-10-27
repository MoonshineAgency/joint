#ifndef ESP_IOT_NODE_PLUS_SYSTEM_H_
#define ESP_IOT_NODE_PLUS_SYSTEM_H_

#include <esp_err.h>
#include <esp_log.h>

typedef enum
{
    MODE_INIT = 0,
    MODE_BOOT,
    MODE_SAFE,
    MODE_OFFLINE,
    MODE_ONLINE,

    MODE_MAX
} system_mode_t;

esp_err_t system_init();

void system_set_mode(system_mode_t mode);

const char *system_mode_name(system_mode_t val);

system_mode_t system_mode();

#define SYSTEM_CHECK(x)                                                         \
    do {                                                                        \
        esp_err_t __;                                                           \
        if ((__ = (x)) != ESP_OK) {                                             \
            ESP_LOGE(TAG, "System failure %d (%s)", __, esp_err_to_name(__));   \
            ESP_ERROR_CHECK(__);                                                \
        }                                                                       \
    } while (0)

#endif // ESP_IOT_NODE_PLUS_SYSTEM_H_
