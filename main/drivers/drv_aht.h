#ifndef ESP_IOT_NODE_PLUS_DRV_AHT_H_
#define ESP_IOT_NODE_PLUS_DRV_AHT_H_

#include "driver.h"
#include <aht.h>

class aht_driver_t : public periodic_driver_t
{
    using periodic_driver_t::periodic_driver_t;

private:
    std::unordered_map<std::string, aht_t> _devices;

protected:
    void loop() override;

public:
    esp_err_t init() override;
    esp_err_t done() override;
};

#endif // ESP_IOT_NODE_PLUS_DRV_AHT_H_
