#include "hwrtc.h"
#include "common.h"
#include <sys/time.h>
#include <errno.h>

#if defined(HW_RTC_PCF8563)
#include "hwrtc/rtc_pcf8563.h"
#elif defined(HW_RTC_DS3231)
#include "hwrtc/rtc_ds3231.h"
#else
#define HW_RTC_NONE
#endif

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
    log_localtime();
    return ESP_OK;
#endif

    struct tm ti = { 0 };

    hwrtc_init(&ti);

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

    hwrtc_update(&t);

    log_localtime();

    return ESP_OK;
}
