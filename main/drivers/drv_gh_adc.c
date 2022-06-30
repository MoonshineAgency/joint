#include "drv_gh_adc.h"
#include "mqtt.h"
#include "periodic_driver.h"
#include <driver/adc.h>
#include <esp_adc_cal.h>

static esp_adc_cal_characteristics_t adc_cal = { 0 };
static size_t samples;

static void loop(driver_t *self)
{
    uint32_t v0 = 0, v1 = 0, v2 = 0, v3 = 0;

    for (size_t i = 0; i < samples; i++)
    {
        uint32_t v;
        esp_adc_cal_get_voltage(ADC1_CHANNEL_4, &adc_cal, &v);
        v0 += v;
        esp_adc_cal_get_voltage(ADC1_CHANNEL_5, &adc_cal, &v);
        v1 += v;
        esp_adc_cal_get_voltage(ADC1_CHANNEL_6, &adc_cal, &v);
        v2 += v;
        esp_adc_cal_get_voltage(ADC1_CHANNEL_7, &adc_cal, &v);
        v3 += v;
    }

    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "AIN0", v0 / samples);
    cJSON_AddNumberToObject(json, "AIN1", v1 / samples);
    cJSON_AddNumberToObject(json, "AIN2", v2 / samples);
    cJSON_AddNumberToObject(json, "AIN3", v3 / samples);
    mqtt_publish_json_subtopic("ADC", json, 0, 0);
    cJSON_Delete(json);
}

static esp_err_t init(driver_t *self)
{
    self->internal[0] = loop;

    adc_atten_t atten = config_get_int(cJSON_GetObjectItem(self->config, "attenuation"), ADC_ATTEN_DB_11);
    samples = config_get_int(cJSON_GetObjectItem(self->config, "samples"), 64);

    adc_power_acquire();
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_4, atten);
    adc1_config_channel_atten(ADC1_CHANNEL_5, atten);
    adc1_config_channel_atten(ADC1_CHANNEL_6, atten);
    adc1_config_channel_atten(ADC1_CHANNEL_7, atten);

    esp_adc_cal_characterize(ADC_UNIT_1, atten, ADC_WIDTH_BIT_12, 1100, &adc_cal);

    return periodic_driver_init(self);
}

driver_t drv_gh_adc = {
    .name = "drv_gh_adc",

    .config = NULL,
    .state = DRIVER_NEW,
    .context = NULL,
    .internal = { 0 },

    .init = init,
    .start = periodic_driver_start,
    .suspend = periodic_driver_suspend,
    .resume = periodic_driver_resume,
    .stop = periodic_driver_stop,
};
