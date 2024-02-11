#include "system_clock.h"
#include <time.h>
#include <errno.h>
#include <esp_sntp.h>
#include "common.h"
#include "settings.h"

#if defined(HW_RTC_PCF8563)
#include "hwrtc/hwrtc_pcf8563.h"
#elif defined(HW_RTC_DS3231)
#include "hwrtc/hwrtc_ds3231.h"
#else
#include "hwrtc/hwrtc_none.h"
#endif

static void log_time()
{
    char buf[64];

    time_t t = time(NULL);

    strftime(buf, sizeof(buf) - 1, DEFAULT_DT_FMT, gmtime(&t));
    ESP_LOGI(TAG, "Global time: %s UTC", buf);

    strftime(buf, sizeof(buf) - 1, DEFAULT_DT_FMT, localtime(&t));
    ESP_LOGI(TAG, "Local time:  %s %s", buf, settings.sntp.tz);

    struct tm rtc_time = { 0 };
    esp_err_t r = hw_rtc_get_time(&rtc_time);
    if (r != ESP_OK)
    {
        ESP_LOGW(TAG, "Error while reading HW RTC: %d (%s)", r, esp_err_to_name(r));
        return;
    }
    strftime(buf, sizeof(buf) - 1, DEFAULT_DT_FMT, &rtc_time);
    ESP_LOGI(TAG, "RTC time:    %s", buf);
}

esp_err_t system_clock_init()
{
    setenv("TZ", settings.sntp.tz, 1);
    tzset();

    struct tm time = { 0 };
    CHECK(hw_rtc_init(&time));

    struct timeval t = {
        .tv_sec = mktime(&time),
        .tv_usec = 0,
    };
    if (settimeofday(&t, NULL) != 0)
        ESP_LOGW(TAG, "Error setting localtime: %d", errno);

    log_time();

    return ESP_OK;
}

static void update(struct timeval *tv)
{
    (void)tv;

    ESP_LOGI(TAG, "Got time from SNTP server");

    struct tm t = { 0 };
    time_t now;

    time(&now);
    localtime_r(&now, &t);
    hw_rtc_set_time(&t);
    log_time();
}

void system_clock_sntp_init()
{
    if (!settings.sntp.enabled)
    {
        ESP_LOGW(TAG, "SNTP client disabled");
        return;
    }

    if (!strlen(settings.sntp.time_server))
    {
        ESP_LOGE(TAG, "Invalid time server");
        return;
    }

    if (esp_sntp_enabled())
        return;

    ESP_LOGI(TAG, "Initializing SNTP client...");

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_set_sync_interval(settings.sntp.interval * 1000);
    esp_sntp_setservername(0, settings.sntp.time_server);
    sntp_set_time_sync_notification_cb(update);
    esp_sntp_init();

    ESP_LOGI(TAG, "SNTP client is running");
}
