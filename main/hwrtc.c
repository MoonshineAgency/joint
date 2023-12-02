#include "hwrtc.h"
#include "common.h"
#include <sys/time.h>
#include <errno.h>
#include "settings.h"

#if defined(HW_RTC_PCF8563)
#include "hwrtc/rtc_pcf8563.h"
#elif defined(HW_RTC_DS3231)
#include "hwrtc/rtc_ds3231.h"
#else
#define HW_RTC_NONE
#endif

static char buf[64];

static void log_time()
{
    time_t t = time(NULL);

    strftime(buf, sizeof(buf) - 1, DEFAULT_DT_FMT, gmtime(&t));
    ESP_LOGI(TAG, "Global time: UTC %s", buf);

    strftime(buf, sizeof(buf) - 1, DEFAULT_DT_FMT, localtime(&t));
    ESP_LOGI(TAG, "Local time: %s %s", settings.sntp.tz, buf);

#ifndef HW_RTC_NONE
    struct tm rtc_time;
    esp_err_t r = rtc_get_time(&rtc_time);
    if (r != ESP_OK)
    {
        ESP_LOGW(TAG, "Error while reading hardware RTC: %d (%s)", r, esp_err_to_name(r));
        return;
    }
    strftime(buf, sizeof(buf) - 1, DEFAULT_DT_FMT, &rtc_time);
    ESP_LOGI(TAG, "RTC time: %s", buf);
#endif
}

static esp_err_t set_localtime(struct tm *value)
{
    struct timeval t = {
        .tv_sec = mktime(value),
        .tv_usec = 0
    };
    if (settimeofday(&t, NULL) != 0)
    {
        ESP_LOGW(TAG, "Error setting local time: %d", errno);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t hw_rtc_init()
{
    setenv("TZ", settings.sntp.tz, 1);
    tzset();

#ifdef HW_RTC_NONE
    ESP_LOGW(TAG, "Hardware RTC is not available");
    log_time();
    return ESP_OK;
#endif

    struct tm ti = { 0 };

    rtc_init(&ti);
    set_localtime(&ti);
    log_time();

    return ESP_OK;
}

esp_err_t hw_rtc_update()
{
#ifdef HW_RTC_NONE
    ESP_LOGW(TAG, "Hardware RTC is not available");
    log_time();
    return ESP_OK;
#endif

    struct tm t;
    time_t now;

    time(&now);
    localtime_r(&now, &t);
    rtc_update(&t);
    log_time();

    return ESP_OK;
}
