#include "drv_ph_meter.h"
#include "periodic_driver.h"
#include "mqtt.h"
#include <esp_check.h>
#include <ads111x.h>

#define GAIN ADS111X_GAIN_0V512

static i2c_dev_t dev = { 0 };
static int samples;
static float gain;

static inline bool wait_adc_busy(driver_t *self)
{
    bool busy;
    do
    {
        esp_err_t r = ads111x_is_busy(&dev, &busy);
        if (r != ESP_OK)
        {
            ESP_LOGE(self->name, "Device is not responding: %d (%s)", r, esp_err_to_name(r));
            return false;
        }
    }
    while (busy);

    return true;
}

static void loop(driver_t *self)
{
    float voltage = 0;
    int32_t raw = 0;
    for (int i = 0; i < samples; i++)
    {
        esp_err_t r = ads111x_start_conversion(&dev);
        if (r != ESP_OK)
        {
            ESP_LOGE(self->name, "Error starting conversion: %d (%s)", r, esp_err_to_name(r));
            return;
        }
        if (!wait_adc_busy(self))
            return;

        int16_t v;
        r = ads111x_get_value(&dev, &v);
        if (r != ESP_OK)
        {
            ESP_LOGE(self->name, "Error reading ADC value: %d (%s)", r, esp_err_to_name(r));
            return;
        }

        raw += v;
        voltage += gain * (float)v;
    }
    voltage /= (float)samples;
    raw /= samples;

    cJSON *json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "voltage", voltage);
    cJSON_AddNumberToObject(json, "raw", raw);
    mqtt_publish_json_subtopic("pH", json, 0, 0);
    cJSON_Delete(json);
}

static esp_err_t init(driver_t *self)
{
    self->internal[0] = loop;

    gpio_num_t sda = config_get_gpio(cJSON_GetObjectItem(self->config, "sda"), CONFIG_I2C0_SDA_GPIO);
    gpio_num_t scl = config_get_gpio(cJSON_GetObjectItem(self->config, "scl"), CONFIG_I2C0_SCL_GPIO);
    uint8_t addr = config_get_int(cJSON_GetObjectItem(self->config, "address"), ADS111X_ADDR_GND);
    i2c_port_t port = config_get_int(cJSON_GetObjectItem(self->config, "port"), 0);
    int freq = config_get_int(cJSON_GetObjectItem(self->config, "frequency"), 0);
    samples = config_get_int(cJSON_GetObjectItem(self->config, "samples"), 32);

    gain = ads111x_gain_values[GAIN] / ADS111X_MAX_VALUE;

    ESP_RETURN_ON_ERROR(
        ads111x_init_desc(&dev, addr, port, sda, scl),
        self->name, "Error initializing device descriptor: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );
    if (freq)
        dev.cfg.master.clk_speed = freq;
    ESP_RETURN_ON_ERROR(
        ads111x_set_gain(&dev, GAIN),
        self->name, "Error setting gain: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );
    ESP_RETURN_ON_ERROR(
        ads111x_set_input_mux(&dev, ADS111X_MUX_0_1),
        self->name, "Error setting input MUX: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );
    ESP_RETURN_ON_ERROR(
        ads111x_set_mode(&dev, ADS111X_MODE_SINGLE_SHOT),
        self->name, "Error setting mode: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );

    return periodic_driver_init(self);
}


driver_t drv_ph_meter = {
    .name = "drv_ph_meter",

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
