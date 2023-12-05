#ifndef ESP_IOT_NODE_PLUS_DRV_ADC_H_
#define ESP_IOT_NODE_PLUS_DRV_ADC_H_

#include "common.h"

#ifdef DRIVER_GH_ADC

/*
{
  "period": 2000,           // ms
  "samples": 64,
  "attenuation": 3,         // 0 - no attenuation (100 mV ~ 950 mV), 1 - 2.5 dB (100 mV ~ 1250 mV),
                            // 2 - 6 dB (150 mV ~ 1750 mV), 3 - 12 dB (150 mV ~ 2450 mV)
  "moisture": true,         // enable/disable soil moisture sensors
  "moisture_calibration": [ // optional if soil moisture sensors are disabled
    {
      "voltage": 2.2,
      "moisture": 0
    },
    {
      "voltage": 0.9,
      "moisture": 100
    }
  ],
  "tds_calibration": [
    {
      "voltage": 0,
      "tds": 0
    },
    {
      "voltage": 1,
      "tds": 1
    }
  ]
}
*/

#include "driver.h"
#include "std_strings.h"

extern driver_t drv_gh_adc;

#endif

#endif // ESP_IOT_NODE_PLUS_DRV_ADC_H_
