#include "hwrtc.h"
#include "common.h"
#include <pcf8563.h>
#include <sys/time.h>
#include <errno.h>

static i2c_dev_t dev = { 0 };
static bool hw_rtc_valid = false;
static char buf[64];

static void log_localtime()
{
    time_t now;
    struct tm ti;

    time(&now);
    localtime_r(&now, &ti);
    strftime(buf, sizeof(buf), "%c", &ti);

    ESP_LOGI(TAG, "Local time (%s): %s", CONFIG_NODE_TZ, buf);
}

esp_err_t hw_rtc_init()
{
    setenv("TZ", CONFIG_NODE_TZ, 1);
    tzset();

#ifdef HW_RTC_NONE
    ESP_LOGW(TAG, "Hardware RTC is not available");
#endif

    struct tm ti = { 0 };

#ifdef HW_RTC_PCF8574
    CHECK(pcf8563_init_desc(&dev, HW_INTERNAL_PORT, HW_INTERNAL_SDA_GPIO, HW_INTERNAL_SCL_GPIO));

    bool valid;

    CHECK(pcf8563_get_time(&dev, &ti, &valid));
    if (!valid)
    {
        ESP_LOGW(TAG, "RTC time is invalid, please reset");
        return ESP_ERR_INVALID_STATE;
    }
#endif

#ifdef HW_RTC_PCF8574
    struct timeval t = {
        .tv_sec = mktime(&ti),
        .tv_usec = 0
    };
    if (settimeofday(&t, NULL) != 0)
    {
        ESP_LOGW(TAG, "Error setting local time: %d", errno);
        return ESP_FAIL;
    }
    hw_rtc_valid = true;
#endif

    log_localtime();

    return ESP_OK;
}

esp_err_t hw_rtc_update()
{
#ifdef HW_RTC_NONE
    ESP_LOGW(TAG, "Hardware RTC is not available");

    log_localtime();

    return ESP_OK;
#endif

    if (!hw_rtc_valid)
    {
        ESP_LOGE(TAG, "RTC is not available");
        return ESP_ERR_NOT_FOUND;
    }

    struct tm t;
    time_t now;

    time(&now);
    gmtime_r(&now, &t);

#ifdef HW_RTC_PCF8574
    CHECK(pcf8563_set_time(&dev, &t));
#endif

    log_localtime();

    return ESP_OK;
}
