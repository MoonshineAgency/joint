#ifndef ESP_IOT_NODE_PLUS_DRV_AHT_H_
#define ESP_IOT_NODE_PLUS_DRV_AHT_H_

#include "common.h"

#ifdef DRIVER_AHTXX
/*
{
  "period": 5000, // ms
  "samples": 8,
  "temp_bad_threshold": 120,
  "sensors": [
    {
      "type": 0,     // 0 - AHT10/AHT15, 1 - AHT20
      "address": 56,
      "frequency": 100000 // I2C frequency, Hz, optional
    }
  ]
}
*/
#include "driver.h"
#include "std_strings.h"

extern driver_t drv_aht;

#endif

#endif // ESP_IOT_NODE_PLUS_DRV_AHT_H_
