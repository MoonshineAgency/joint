#ifndef __RTC_PCF8574_H__
#define __RTC_PCF8574_H__

#include "common.h"
#include <sys/time.h>
#include <ds3231.h>

static i2c_dev_t dev = { 0 };

static inline esp_err_t rtc_get_time(struct tm *time)
{
    CHECK_ARG(time);

    return ds3231_get_time(&dev, time);
}

static inline esp_err_t rtc_init(struct tm *time)
{
    CHECK(ds3231_init_desc(&dev, HW_INTERNAL_PORT, HW_INTERNAL_SDA_GPIO, HW_INTERNAL_SCL_GPIO));

    return rtc_get_time(time);
}

static inline esp_err_t rtc_update(const struct tm *time)
{
    CHECK_ARG(time);

    return ds3231_set_time(&dev, (struct tm *)time);
}

#endif /* __RTC_PCF8574_H__ */
