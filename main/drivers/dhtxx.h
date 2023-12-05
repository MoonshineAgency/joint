#ifndef ESP_IOT_NODE_PLUS_DRV_DHTXX_H_
#define ESP_IOT_NODE_PLUS_DRV_DHTXX_H_

#include "common.h"

#ifdef DRIVER_DHTXX

/*
{
  "period": 5000,            // ms
  "sensors": [
    {
        "gpio": 21,          // GPIO number
        "type": 0..2,        // sensor type: 0 -  dht11, 1 - dht22, 2 - Itead Si7021
        "pullup": false,     // enable/disable internal pull-up
    }
  ]
}
*/
#include "driver.h"
#include "std_strings.h"

extern driver_t drv_dht;

#endif

#endif // ESP_IOT_NODE_PLUS_DRV_DHTXX_H_
