#ifndef ESP_IOT_NODE_PLUS_DRV_DHTXX_H_
#define ESP_IOT_NODE_PLUS_DRV_DHTXX_H_

#include "driver.h"
#include "std_strings.h"

#define DHT_DEFCONFIG "{ \"" OPT_STACK_SIZE "\": 4096, \"" OPT_PERIOD "\": 5000, \"" OPT_SENSORS \
    "\": [{ \"" OPT_GPIO "\": 21, \"" OPT_TYPE "\": 0, \"" OPT_PULLUP "\": true }] }"

extern driver_t drv_dht;

#endif // ESP_IOT_NODE_PLUS_DRV_DHTXX_H_
