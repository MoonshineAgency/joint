#ifndef __RTC_DS3231_H__
#define __RTC_DS3231_H__

#include <esp_err.h>
#include <time.h>

static inline esp_err_t hw_rtc_get_time(struct tm *time)
{
    return ESP_OK;
}

static inline esp_err_t hw_rtc_init(struct tm *time)
{
    return ESP_OK;
}

static inline esp_err_t hw_rtc_set_time(const struct tm *time)
{
    return ESP_OK;
}

#endif /* __RTC_DS3231_H__ */
