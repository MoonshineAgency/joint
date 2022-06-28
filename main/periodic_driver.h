#ifndef ESP_IOT_NODE_PLUS_PERIODIC_DRIVER_H_
#define ESP_IOT_NODE_PLUS_PERIODIC_DRIVER_H_

#include "driver.h"

typedef void (*periodic_loop_callback_t)(driver_t *self);

esp_err_t periodic_driver_init(driver_t *drv);
esp_err_t periodic_driver_start(driver_t *drv);
esp_err_t periodic_driver_suspend(driver_t *drv);
esp_err_t periodic_driver_resume(driver_t *drv);
esp_err_t periodic_driver_stop(driver_t *drv);

#endif // ESP_IOT_NODE_PLUS_PERIODIC_DRIVER_H_
