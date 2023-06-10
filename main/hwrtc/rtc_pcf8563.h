#ifndef __RTC_PCF8574_H__
#define __RTC_PCF8574_H__

#include "common.h"
#include <sys/time.h>
#include <pcf8563.h>

static i2c_dev_t dev = { 0 };

static inline esp_err_t hwrtc_init(struct tm *time)
{
    CHECK_ARG(time);

    CHECK(pcf8563_init_desc(&dev, HW_INTERNAL_PORT, HW_INTERNAL_SDA_GPIO, HW_INTERNAL_SCL_GPIO));

    bool valid;

    CHECK(pcf8563_get_time(&dev, time, &valid));
    if (!valid)
    {
        ESP_LOGW(TAG, "RTC time is invalid, please reset");
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}

static inline esp_err_t hwrtc_update(const struct tm *time)
{
    CHECK_ARG(time);

    return pcf8563_set_time(&dev, (struct tm *)time);
}

#endif /* __RTC_PCF8574_H__ */
