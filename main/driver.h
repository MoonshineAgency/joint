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

esp_err_t driver_init(driver_t *drv, const char *config);
esp_err_t driver_start(driver_t *drv);
esp_err_t driver_suspend(driver_t *drv);
esp_err_t driver_resume(driver_t *drv);
esp_err_t driver_stop(driver_t *drv);

int config_get_int(cJSON *item, int def);
gpio_num_t config_get_gpio(cJSON *item);

#endif // ESP_IOT_NODE_PLUS_DRIVER_H_
