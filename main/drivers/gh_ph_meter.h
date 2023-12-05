#ifndef ESP_IOT_NODE_PLUS_DRV_PH_METER_H_
#define ESP_IOT_NODE_PLUS_DRV_PH_METER_H_

#include "common.h"

#ifdef DRIVER_GH_PH_METER

/*
{
  "period": 5000,         // ms
  "samples": 32,
  "calibration": [
    {
      "voltage": 0,
      "ph": 7
    },
    {
      "voltage": 0.17143,
      "ph": 4.01
    }
  ]
}
*/

#include "driver.h"
#include "std_strings.h"

extern driver_t drv_ph_meter;

#endif

#endif // ESP_IOT_NODE_PLUS_DRV_PH_METER_H_
