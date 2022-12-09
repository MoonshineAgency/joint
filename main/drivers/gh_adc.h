#ifndef ESP_IOT_NODE_PLUS_DRV_ADC_H_
#define ESP_IOT_NODE_PLUS_DRV_ADC_H_

#include "driver.h"
#include "std_strings.h"

#define GH_ADC_DEFCONFIG "{ \"" OPT_STACK_SIZE "\": 4096, \"" OPT_PERIOD "\": 2000, \"" OPT_SAMPLES "\": 64, \"" OPT_ATTEN "\": 3, " \
    "\"" OPT_MOISTURE "\": { \"voltage_0\": 2.2, \"voltage_100\": 0.9, \"" OPT_ENABLED "\": true } }"

extern driver_t drv_gh_adc;

#endif // ESP_IOT_NODE_PLUS_DRV_ADC_H_
