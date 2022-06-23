#ifndef ESP_IOT_NODE_PLUS_DRV_DS18X20_H_
#define ESP_IOT_NODE_PLUS_DRV_DS18X20_H_

#include "driver.h"
#include <array>
#include <ds18x20.h>

#define CONFIG_DS18X20_MAX_SENSORS 64

class ds18x20_driver_t : public periodic_driver_t
{
    using periodic_driver_t::periodic_driver_t;

private:
    std::array<ds18x20_addr_t, CONFIG_DS18X20_MAX_SENSORS> _sensors;
    std::array<float, CONFIG_DS18X20_MAX_SENSORS> _results;
    gpio_num_t _gpio;
    size_t _rescan = 0;
    size_t _sensors_found = 0;
    size_t _loop_no = 0;

protected:
    void loop() override;

public:
    esp_err_t init() override;
};

#endif // ESP_IOT_NODE_PLUS_DRV_DS18X20_H_
