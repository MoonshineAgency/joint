#include "network.h"
#include "wifi.h"
#include "system.h"

esp_err_t network_init()
{
    return wifi_init();
}
