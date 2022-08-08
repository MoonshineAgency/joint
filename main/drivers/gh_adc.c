#include "gh_adc.h"
#include <esp_log.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
#include "settings.h"

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
static int update_period;

static esp_err_t on_init(driver_t *self)
{
    cvector_free(self->devices);

    adc_atten_t atten = driver_config_get_int(cJSON_GetObjectItem(self->config, "attenuation"), ADC_ATTEN_DB_11);
    samples = driver_config_get_int(cJSON_GetObjectItem(self->config, "samples"), 64);
    update_period = driver_config_get_int(cJSON_GetObjectItem(self->config, "period"), 1000);

    //adc_power_acquire();
    adc1_config_width(ADC_WIDTH);

    for (size_t c = 0; c < AIN_COUNT; c++)
        adc1_config_channel_atten(ain_channels[c], atten);
    esp_adc_cal_characterize(ADC_UNIT_1, atten, ADC_WIDTH, 1100, &ain_cal);

    adc1_config_channel_atten(CONFIG_ADC_DRIVER_TDS_CHANNEL, TDS_ATTEN);
    esp_adc_cal_characterize(ADC_UNIT_1, TDS_ATTEN, ADC_WIDTH, 1100, &tds_cal);

    device_t dev = { 0 };
    for (size_t c = 0; c < AIN_COUNT; c++)
    {
        memset(&dev, 0, sizeof(dev));
        dev.type = DEV_SENSOR;
        dev.sensor.precision = 3;
        dev.sensor.update_period = update_period;
        strncpy(dev.sensor.measurement_unit, "V", sizeof(dev.sensor.measurement_unit));
        strncpy(dev.device_class, "voltage",  sizeof(dev.device_class));
        snprintf(dev.uid, sizeof(dev.uid), "ain%d", c);
        snprintf(dev.name, sizeof(dev.name), "%s analog input %d", settings.node.name, c);
        cvector_push_back(self->devices, dev);
    }

    for (size_t c = 0; c < AIN_COUNT; c++)
    {
        memset(&dev, 0, sizeof(dev));
        dev.type = DEV_SENSOR;
        dev.sensor.precision = 1;
        dev.sensor.update_period = update_period;
        strncpy(dev.sensor.measurement_unit, "%", sizeof(dev.sensor.measurement_unit));
        strncpy(dev.device_class, "humidity", sizeof(dev.device_class));
        snprintf(dev.uid, sizeof(dev.uid), "moisture%d", c);
        snprintf(dev.name, sizeof(dev.name), "%s moisture sensor on AIN%d", settings.node.name, c);
        cvector_push_back(self->devices, dev);
    }

    memset(&dev, 0, sizeof(dev));
    dev.type = DEV_SENSOR;
    dev.sensor.precision = 3;
    dev.sensor.update_period = update_period;
    strncpy(dev.sensor.measurement_unit, "V", sizeof(dev.sensor.measurement_unit));
    strncpy(dev.device_class, "voltage",  sizeof(dev.device_class));
    strncpy(dev.uid, "tds",  sizeof(dev.uid));
    snprintf(dev.name, sizeof(dev.name), "%s TDS input raw", settings.node.name);
    cvector_push_back(self->devices, dev);

    return ESP_OK;
}

static void task(driver_t *self)
{
    esp_err_t r;

    uint32_t ain_voltages[AIN_COUNT];
    uint32_t tds_voltage;

    TickType_t period = pdMS_TO_TICKS(update_period);
    float moisture_0 = driver_config_get_float(cJSON_GetObjectItem(self->config, "moisture_0"), 2.2f);
    float moisture_100 = driver_config_get_float(cJSON_GetObjectItem(self->config, "moisture_100"), 0.9f);
    float moisture_perc = (moisture_0 - moisture_100) / 100.0f;

    while (true)
    {
        TickType_t start = xTaskGetTickCount();

        memset(ain_voltages, 0, sizeof(ain_voltages));
        tds_voltage = 0;

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

        for (size_t c = 0; c < AIN_COUNT; c++)
        {
            float voltage = (float)ain_voltages[c] / (float)samples / 1000.0f;
            self->devices[c].sensor.value = voltage;
            driver_send_device_update(self, &self->devices[c]);
        }

        for (size_t c = 0; c < AIN_COUNT; c++)
        {
            float voltage = (float)ain_voltages[c] / (float)samples / 1000.0f;
            float moisture;
            if (voltage <= moisture_100)
                moisture = 100;
            else if (voltage >= moisture_0)
                moisture = 0;
            else
                moisture = (moisture_0 - voltage) / moisture_perc;
            self->devices[c + AIN_COUNT].sensor.value = moisture;
            driver_send_device_update(self, &self->devices[c + AIN_COUNT]);
        }

        self->devices[AIN_COUNT * 2].sensor.value = (float)tds_voltage / (float)samples / 1000.0f;
        driver_send_device_update(self, &self->devices[AIN_COUNT * 2]);

        while (xTaskGetTickCount() - start < period)
        {
            if (!(xEventGroupGetBits(self->eg) & DRIVER_BIT_START))
                return;
            vTaskDelay(1);
        }
    }
}

static esp_err_t on_stop(driver_t *self)
{
    //adc_power_release();
    return ESP_OK;
}

driver_t drv_gh_adc = {
    .name = "gh_adc",
    .defconfig = "{ \"stack_size\": 4096, \"period\": 2000, \"samples\": 64, \"attenuation\": 3, \"moisture_0\": 2.2, \"moisture_100\": 0.9 }",

    .config = NULL,
    .state = DRIVER_NEW,
    .event_queue = NULL,

    .devices = NULL,
    .handle = NULL,
    .eg = NULL,

    .on_init = on_init,
    .on_start = NULL,
    .on_stop = on_stop,

    .task = task
};
