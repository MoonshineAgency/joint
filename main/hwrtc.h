#ifndef ESP_IOT_NODE_PLUS_RTC_H_
#define ESP_IOT_NODE_PLUS_RTC_H_

#include <esp_err.h>
#include <time.h>

esp_err_t hw_rtc_init();

esp_err_t hw_rtc_update();

#endif // ESP_IOT_NODE_PLUS_RTC_H_
