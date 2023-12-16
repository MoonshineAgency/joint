#ifndef ESP_IOT_NODE_PLUS_DRIVER_H_
#define ESP_IOT_NODE_PLUS_DRIVER_H_

#include <esp_err.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <cJSON.h>
#include <device.h>
#include <cvector.h>
#include <calibration.h>

#define DRIVER_BIT_INITIALIZED BIT(0)
#define DRIVER_BIT_RUNNING     BIT(1)
#define DRIVER_BIT_STOPPED     BIT(2)
#define DRIVER_BIT_START       BIT(3)

typedef enum {
    DRIVER_NEW = 0,
    DRIVER_INITIALIZED,
    DRIVER_RUNNING,
    DRIVER_FINISHED,
    DRIVER_INVALID
} driver_state_t;

typedef struct driver driver_t;

typedef esp_err_t (*driver_cb_t)(driver_t *self);
typedef void (*driver_loop_cb_t)(driver_t *self);
typedef void (*driver_write_cb_t)(driver_t *self, device_t *dev, const char *payload, size_t len);

struct driver
{
    char name[32];
    const char *defconfig;
    uint32_t stack_size;
    UBaseType_t priority;
    cJSON *config;
    driver_state_t state;
    QueueHandle_t event_queue;

    cvector_vector_type(device_t) devices;

    TaskHandle_t handle;
    EventGroupHandle_t eg;

    driver_cb_t on_init;
    driver_cb_t on_start;
    driver_cb_t on_stop;

    driver_write_cb_t on_write;

    driver_loop_cb_t task;
};

typedef enum {
    DRV_EVENT_DEVICE_UPDATED = 0,
    DRV_EVENT_DEVICE_ADDED,
    DRV_EVENT_DEVICE_REMOVED,
} driver_event_type_t;

typedef struct {
    driver_event_type_t type;
    driver_t *sender;
    device_t dev; // copy
} driver_event_t;

esp_err_t driver_init(driver_t *drv, const char *config, size_t cfg_len);
esp_err_t driver_start(driver_t *drv);
esp_err_t driver_stop(driver_t *drv);

void driver_send_device_update(driver_t *drv, const device_t *dev);
void driver_send_device_add(driver_t *drv, const device_t *dev);
void driver_send_device_remove(driver_t *drv, const device_t *dev);

int driver_config_get_int(cJSON *item, int def);
gpio_num_t driver_config_get_gpio(cJSON *item, gpio_num_t def);
bool driver_config_get_bool(cJSON *item, bool def);
esp_err_t driver_config_read_calibration(const char *tag, cJSON *item, calibration_handle_t *c,
    const calibration_point_t *def, size_t def_points);

#endif // ESP_IOT_NODE_PLUS_DRIVER_H_
