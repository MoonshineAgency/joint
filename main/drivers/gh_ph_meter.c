#include "gh_ph_meter.h"
#include <esp_check.h>
#include <esp_timer.h>
#include <ads111x.h>
#include "settings.h"

#define GAIN ADS111X_GAIN_0V512

static i2c_dev_t adc = { 0 };
static int samples;
static float gain;
static float e1, slope;
static float ph;
static uint32_t last_update_time;
static int update_period;

static esp_err_t on_init(driver_t *self)
{
    cvector_free(self->devices);

    gpio_num_t sda = driver_config_get_gpio(cJSON_GetObjectItem(self->config, "sda"), CONFIG_I2C0_SDA_GPIO);
    gpio_num_t scl = driver_config_get_gpio(cJSON_GetObjectItem(self->config, "scl"), CONFIG_I2C0_SCL_GPIO);
    uint8_t addr = driver_config_get_int(cJSON_GetObjectItem(self->config, "address"), ADS111X_ADDR_GND);
    i2c_port_t port = driver_config_get_int(cJSON_GetObjectItem(self->config, "port"), 0);
    int freq = driver_config_get_int(cJSON_GetObjectItem(self->config, "frequency"), 0);
    samples = driver_config_get_int(cJSON_GetObjectItem(self->config, "samples"), 32);
    update_period = driver_config_get_int(cJSON_GetObjectItem(self->config, "period"), 1000);

    e1 = driver_config_get_float(cJSON_GetObjectItem(self->config, "ph7_voltage"), 0.0f);
    float e2 = driver_config_get_float(cJSON_GetObjectItem(self->config, "ph4_voltage"), 0.17143f);

    slope = (4.0f - 7.0f) / (e2 - e1);

    gain = ads111x_gain_values[GAIN] / ADS111X_MAX_VALUE;

    memset(&adc, 0, sizeof(adc));
    ESP_RETURN_ON_ERROR(
        ads111x_init_desc(&adc, addr, port, sda, scl),
        self->name, "Error initializing device descriptor: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );
    if (freq)
        adc.cfg.master.clk_speed = freq;
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
    snprintf(dev.uid, sizeof(dev.uid), "ph0");
    dev.type = DEV_SENSOR;
    snprintf(dev.name, sizeof(dev.name), "%s pH 0", settings.node.name);
    strncpy(dev.sensor.measurement_unit, "pH", sizeof(dev.sensor.measurement_unit));
    dev.sensor.precision = 2;
    dev.sensor.update_period = update_period;
    cvector_push_back(self->devices, dev);

    memset(&dev, 0, sizeof(dev));
    snprintf(dev.uid, sizeof(dev.uid), "ph0_raw");
    dev.type = DEV_SENSOR;
    snprintf(dev.name, sizeof(dev.name), "%s pH 0 raw voltage", settings.node.name);
    strncpy(dev.device_class, "voltage", sizeof(dev.device_class));
    strncpy(dev.sensor.measurement_unit, "V", sizeof(dev.sensor.measurement_unit));
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
    .defconfig = "{ \"stack_size\": 4096, \"period\": 5000, \"port\": 0, \"sda\": 16, \"scl\": 17, "
                 "\"address\": 72, \"ph7_voltage\": 0, \"ph4_voltage\": 0.17143, \"samples\": 32 }",

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
