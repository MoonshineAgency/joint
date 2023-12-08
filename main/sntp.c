#include "sntp.h"
#include "common.h"
#include "hwrtc.h"
#include "settings.h"
#include <esp_sntp.h>

static void callback(struct timeval *tv)
{
    ESP_LOGI(TAG, "Got time from SNTP, updating HW RTC");
    esp_err_t r = hw_rtc_update();
    if (r != ESP_OK)
        ESP_LOGW(TAG, "Error updating HW RTC: %d (%s)", r, esp_err_to_name(r));
}

void sntp_iot_init()
{
    if (!settings.sntp.enabled)
    {
        ESP_LOGW(TAG, "SNTP client disabled");
        return;
    }

    if (!strlen(settings.sntp.time_server))
    {
        ESP_LOGE(TAG, "Invalid SNTP time server");
        return;
    }

    ESP_LOGI(TAG, "Initializing SNTP");

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_set_sync_interval(settings.sntp.interval * 1000);
    esp_sntp_setservername(0, settings.sntp.time_server);
    sntp_set_time_sync_notification_cb(callback);
    esp_sntp_init();
}
