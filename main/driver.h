#ifndef ESP_IOT_NODE_PLUS_DRIVER_H_
#define ESP_IOT_NODE_PLUS_DRIVER_H_

#include <esp_err.h>
#include <driver/gpio.h>
#include <cJSON.h>

typedef enum {
    DRIVER_NEW = 0,
    DRIVER_INITIALIZED,
    DRIVER_RUNNING,
    DRIVER_SUSPENDED,
    DRIVER_FINISHED,
    DRIVER_INVALID
} driver_state_t;

typedef struct driver driver_t;

typedef esp_err_t (*driver_cb_t)(driver_t *self);

struct driver
{
    char name[32];
    const char *defconfig;
    cJSON *config;
    driver_state_t state;

    void *context;
    void *internal[16];

    driver_cb_t init;
    driver_cb_t start;
    driver_cb_t suspend;
    driver_cb_t resume;
    driver_cb_t stop;
};

esp_err_t driver_init(driver_t *drv, const char *config, size_t cfg_len);
esp_err_t driver_start(driver_t *drv);
esp_err_t driver_suspend(driver_t *drv);
esp_err_t driver_resume(driver_t *drv);
esp_err_t driver_stop(driver_t *drv);

const char *driver_state_to_name(driver_state_t state);

int config_get_int(cJSON *item, int def);
float config_get_float(cJSON *item, float def);
gpio_num_t config_get_gpio(cJSON *item, gpio_num_t def);

#endif // ESP_IOT_NODE_PLUS_DRIVER_H_
