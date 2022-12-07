#ifndef ESP_IOT_NODE_PLUS_DRV_AHT_H_
#define ESP_IOT_NODE_PLUS_DRV_AHT_H_

#include "driver.h"
#include "std_strings.h"

#define AHT_DEFCONFIG "{ \"" OPT_STACK_SIZE "\": 4096, \"" OPT_PERIOD "\": 5000, \"" OPT_SENSORS "\": " \
    "[{ \"" OPT_PORT "\": 1, \"" OPT_SDA "\": 13, \"" OPT_SCL "\": 14, \"" OPT_TYPE "\": 0, \"" OPT_ADDRESS "\": 56 }] }"

extern driver_t drv_aht;

#endif // ESP_IOT_NODE_PLUS_DRV_AHT_H_
