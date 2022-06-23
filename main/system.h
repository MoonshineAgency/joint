#ifndef ESP_IOT_NODE_PLUS_SYSTEM_H_
#define ESP_IOT_NODE_PLUS_SYSTEM_H_

#include <esp_err.h>
#include <esp_log.h>

namespace sys {

typedef enum
{
    MODE_INIT = 0,
    MODE_BOOT,
    MODE_SAFE,
    MODE_OFFLINE,
    MODE_ONLINE,

    MODE_MAX
} mode_t;

esp_err_t init();

void set_mode(mode_t mode);

const char *mode_name(mode_t val);

mode_t mode();

#define SYSTEM_CHECK(x)                                                         \
    do {                                                                        \
        esp_err_t __;                                                           \
        if ((__ = (x)) != ESP_OK) {                                             \
            ESP_LOGE(TAG, "System failure %d (%s)", __, esp_err_to_name(__));   \
            ESP_ERROR_CHECK(__);                                                \
        }                                                                       \
    } while (0)

} // namespace sys

#endif // ESP_IOT_NODE_PLUS_SYSTEM_H_
