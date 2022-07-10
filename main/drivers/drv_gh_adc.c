#include "drv_gh_adc.h"
#include "mqtt.h"
#include "periodic_driver.h"
#include <esp_log.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>

#define ADC_WIDTH ADC_WIDTH_BIT_12
#define TDS_ATTEN ADC_ATTEN_DB_6 // 1.75V max
#define AIN_COUNT 4

static esp_adc_cal_characteristics_t ain_cal = { 0 };
static esp_adc_cal_characteristics_t tds_cal = { 0 };
static size_t samples;
static const adc1_channel_t ain_channels[AIN_COUNT] = {
    CONFIG_ADC_DRIVER_AIN0_CHANNEL,
    CONFIG_ADC_DRIVER_AIN1_CHANNEL,
    CONFIG_ADC_DRIVER_AIN2_CHANNEL,
    CONFIG_ADC_DRIVER_AIN3_CHANNEL,
};

static void loop(driver_t *self)
{
    esp_err_t r;

    uint32_t ain_voltages[AIN_COUNT] = {0 };
    uint32_t tds_voltage = 0;

    for (size_t i = 0; i < samples; i++)
    {
        uint32_t v;
        for (size_t c = 0; c < AIN_COUNT; c++)
        {
            v = 0;
            r = esp_adc_cal_get_voltage(ain_channels[c], &ain_cal, &v);
            if (r != ESP_OK)
                ESP_LOGE(self->name, "Error reading ADC1 channel %d: %d (%s)", ain_channels[c], r, esp_err_to_name(r));
            ain_voltages[c] += v;
        }

        v = 0;
        r = esp_adc_cal_get_voltage(CONFIG_ADC_DRIVER_TDS_CHANNEL, &tds_cal, &v);
        if (r != ESP_OK)
            ESP_LOGE(self->name, "Error reading ADC1 channel %d: %d (%s)", CONFIG_ADC_DRIVER_TDS_CHANNEL, r, esp_err_to_name(r));
        tds_voltage += v;
    }

    cJSON *json = cJSON_CreateObject();
    for (size_t c = 0; c < AIN_COUNT; c++)
    {
        char name[8] = { 0 };
        snprintf(name, sizeof(name), "AIN%d", c);
        cJSON_AddNumberToObject(json, name, (float)ain_voltages[c] / samples);
    }
    mqtt_publish_json_subtopic("ADC", json, 0, 0);
    cJSON_Delete(json);

    json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "voltage", (float)tds_voltage / samples / 1000.0f);
    mqtt_publish_json_subtopic("TDS", json, 0, 0);
    cJSON_Delete(json);
}

static esp_err_t init(driver_t *self)
{
    self->internal[0] = loop;

    adc_atten_t atten = config_get_int(cJSON_GetObjectItem(self->config, "attenuation"), ADC_ATTEN_DB_11);
    samples = config_get_int(cJSON_GetObjectItem(self->config, "samples"), 64);

    adc_power_acquire();
    adc1_config_width(ADC_WIDTH);

    for (size_t c = 0; c < AIN_COUNT; c++)
        adc1_config_channel_atten(ain_channels[c], atten);
    esp_adc_cal_characterize(ADC_UNIT_1, atten, ADC_WIDTH, 1100, &ain_cal);

    adc1_config_channel_atten(CONFIG_ADC_DRIVER_TDS_CHANNEL, TDS_ATTEN);
    esp_adc_cal_characterize(ADC_UNIT_1, TDS_ATTEN, ADC_WIDTH, 1100, &tds_cal);

    return periodic_driver_init(self);
}

static esp_err_t stop(driver_t *self)
{
    esp_err_t r = periodic_driver_stop(self);
    adc_power_release();
    return r;
}

driver_t drv_gh_adc = {
    .name = "drv_gh_adc",
    .defconfig = "{ \"stack_size\": 4096, \"period\": 500 }",

    .config = NULL,
    .state = DRIVER_NEW,
    .context = NULL,
    .internal = { 0 },

    .init = init,
    .start = periodic_driver_start,
    .suspend = periodic_driver_suspend,
    .resume = periodic_driver_resume,
    .stop = stop,
};
