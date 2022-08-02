#include "sntp.h"
#include "common.h"
#include "hwrtc.h"
#include <esp_sntp.h>

#ifdef CONFIG_SNTP_ENABLE
static void callback(struct timeval *tv)
{
    ESP_LOGI(TAG, "Got time from SNTP, updating HW RTC");
    setenv("TZ", "UTC", 1);
    tzset();
    esp_err_t r = hw_rtc_update();
    if (r != ESP_OK)
        ESP_LOGW(TAG, "Error updating HW RTC: %d (%s)", r, esp_err_to_name(r));
    setenv("TZ", CONFIG_NODE_TZ, 1);
    tzset();
}
#endif

void sntp_iot_init()
{
#ifdef CONFIG_SNTP_ENABLE
    ESP_LOGI(TAG, "Initializing SNTP");

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, CONFIG_SNTP_TIME_SERVER);
    sntp_set_time_sync_notification_cb(callback);
    sntp_init();
#endif
}
