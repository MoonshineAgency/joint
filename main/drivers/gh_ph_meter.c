#include "gh_ph_meter.h"
#include <esp_check.h>
#include <esp_timer.h>
#include <ads111x.h>
#include "settings.h"
#include "common.h"

#define GAIN ADS111X_GAIN_0V512

static i2c_dev_t adc = { 0 };
static int samples;
static float gain;
static float e1, slope;
static float ph;
static uint32_t last_update_time;
static int update_period;

#define OPT_PH7_VOLTAGE "ph7_voltage"
#define OPT_PH4_VOLTAGE "ph4_voltage"

#define PH_METER_ID  "ph0"
#define PH_RAW_ID    "ph0_raw"

#define FMT_PH_METER_NAME "%s pH 0"
#define FMT_PH_RAW_NAME   "%s pH 0 raw voltage"

#define MU_PH_METER "pH"

static esp_err_t on_init(driver_t *self)
{
    cvector_free(self->devices);

    samples = driver_config_get_int(cJSON_GetObjectItem(self->config, OPT_SAMPLES), 32);
    update_period = driver_config_get_int(cJSON_GetObjectItem(self->config, OPT_PERIOD), 1000);

    e1 = driver_config_get_float(cJSON_GetObjectItem(self->config, OPT_PH7_VOLTAGE), 0.0f);
    float e2 = driver_config_get_float(cJSON_GetObjectItem(self->config, OPT_PH4_VOLTAGE), 0.17143f);

    slope = (4.0f - 7.0f) / (e2 - e1);

    gain = ads111x_gain_values[GAIN] / ADS111X_MAX_VALUE;

    memset(&adc, 0, sizeof(adc));
    ESP_RETURN_ON_ERROR(
        ads111x_init_desc(&adc, DRIVER_GH_PH_METER_ADDRESS, HW_INTERNAL_PORT, HW_INTERNAL_SDA_GPIO, HW_INTERNAL_SCL_GPIO),
        self->name, "Error initializing device descriptor: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );
#if (DRIVER_GH_PH_METER_FREQUENCY)
    adc.cfg.master.clk_speed = DRIVER_GH_PH_METER_FREQUENCY;
#endif
    ESP_RETURN_ON_ERROR(
        ads111x_set_gain(&adc, GAIN),
        self->name, "Error setting gain: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );
    ESP_RETURN_ON_ERROR(
        ads111x_set_input_mux(&adc, ADS111X_MUX_0_1),
        self->name, "Error setting input MUX: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );
    ESP_RETURN_ON_ERROR(
        ads111x_set_mode(&adc, ADS111X_MODE_SINGLE_SHOT),
        self->name, "Error setting mode: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );

    device_t dev = { 0 };
    strncpy(dev.uid, PH_METER_ID, sizeof(dev.uid));
    dev.type = DEV_SENSOR;
    snprintf(dev.name, sizeof(dev.name), FMT_PH_METER_NAME, settings.node.name);
    strncpy(dev.sensor.measurement_unit, MU_PH_METER, sizeof(dev.sensor.measurement_unit));
    dev.sensor.precision = 2;
    dev.sensor.update_period = update_period;
    cvector_push_back(self->devices, dev);

    memset(&dev, 0, sizeof(dev));
    strncpy(dev.uid, PH_RAW_ID, sizeof(dev.uid));
    dev.type = DEV_SENSOR;
    snprintf(dev.name, sizeof(dev.name), FMT_PH_RAW_NAME, settings.node.name);
    strncpy(dev.device_class, DEV_CLASS_VOLTAGE, sizeof(dev.device_class));
    strncpy(dev.sensor.measurement_unit, DEV_MU_VOLTAGE, sizeof(dev.sensor.measurement_unit));
    dev.sensor.precision = 4;
    dev.sensor.update_period = update_period;
    cvector_push_back(self->devices, dev);

    return ESP_OK;
}

static inline bool wait_adc_busy(driver_t *self)
{
    bool busy;
    do
    {
        esp_err_t r = ads111x_is_busy(&adc, &busy);
        if (r != ESP_OK)
        {
            ESP_LOGE(self->name, "Device is not responding: %d (%s)", r, esp_err_to_name(r));
            return false;
        }
        if (busy)
            vTaskDelay(1);
    }
    while (busy);

    return true;
}

static void task(driver_t *self)
{
    uint32_t interval = (esp_timer_get_time() / 1000) - last_update_time;
    last_update_time += interval;

    TickType_t period = pdMS_TO_TICKS(update_period);

    while (true)
    {
        TickType_t start = xTaskGetTickCount();

        float voltage = 0;
        for (int i = 0; i < samples; i++)
        {
            esp_err_t r = ads111x_start_conversion(&adc);
            if (r != ESP_OK)
            {
                ESP_LOGE(self->name, "Error starting conversion: %d (%s)", r, esp_err_to_name(r));
                return;
            }
            if (!wait_adc_busy(self))
                return;

            int16_t v;
            r = ads111x_get_value(&adc, &v);
            if (r != ESP_OK)
            {
                ESP_LOGE(self->name, "Error reading ADC value: %d (%s)", r, esp_err_to_name(r));
                return;
            }

            voltage += gain * (float)v;
        }
        voltage /= (float)samples;

        float dt = (float)interval / 1000.0f;
        float alpha = 1.0f - dt / (dt + 2.0f);
        ph = alpha * ph + (1.0f - alpha) * (7.0f + (voltage - e1) * slope);

        self->devices[0].sensor.value = ph;
        driver_send_device_update(self, &self->devices[0]);
        self->devices[1].sensor.value = voltage;
        driver_send_device_update(self, &self->devices[1]);

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
    esp_err_t r = ads111x_free_desc(&adc);
    if (r != ESP_OK)
        ESP_LOGW(self->name, "Device descriptor free error: %d (%s)", r, esp_err_to_name(r));

    return ESP_OK;
}

driver_t drv_ph_meter = {
    .name = "gh_ph_meter",
    .stack_size = DRIVER_GH_PH_METER_STACK_SIZE,
    .priority = tskIDLE_PRIORITY + 1,
    .defconfig = "{ \"" OPT_PERIOD "\": 5000, \"" OPT_PH7_VOLTAGE "\": 0, \"" OPT_PH4_VOLTAGE "\": 0.17143, \"" OPT_SAMPLES "\": 32 }",

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
