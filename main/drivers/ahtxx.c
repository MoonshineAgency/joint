#include "ahtxx.h"
#include "common.h"
#include "driver.h"
#include "settings.h"
#include <aht.h>

#define FMT_HUMIDITY_SENSOR_ID      "aht_rh%d"
#define FMT_TEMPERATURE_SENSOR_ID   "aht_t%d"

#define FMT_HUMIDITY_SENSOR_NAME    "%s humidity (AHT %d)"
#define FMT_TEMPERATURE_SENSOR_NAME "%s temperature (AHT %d)"

#define OPT_THRESHOLD               "temp_bad_threshold"

static cvector_vector_type(aht_t) sensors = NULL;

static int update_period;
static int samples;
static int threshold;

static esp_err_t on_init(driver_t *self)
{
    cvector_free(self->devices);
    cvector_free(sensors);

    update_period = driver_config_get_int(cJSON_GetObjectItem(self->config, OPT_PERIOD), 1000);
    samples = driver_config_get_int(cJSON_GetObjectItem(self->config, OPT_SAMPLES), 8);
    threshold = driver_config_get_int(cJSON_GetObjectItem(self->config, OPT_THRESHOLD), 120);

    // Init devices
    cJSON *sensors_j = cJSON_GetObjectItem(self->config, OPT_SENSORS);
    for (int i = 0; i < cJSON_GetArraySize(sensors_j); i++)
    {
        cJSON *sensor_j = cJSON_GetArrayItem(sensors_j, i);
        uint8_t addr = driver_config_get_int(cJSON_GetObjectItem(sensor_j, OPT_ADDRESS), AHT_I2C_ADDRESS_GND);
        int freq = driver_config_get_int(cJSON_GetObjectItem(sensor_j, OPT_FREQ), 0);

        aht_t aht;
        memset(&aht, 0, sizeof(aht));
        aht.type = driver_config_get_int(cJSON_GetObjectItem(sensor_j, OPT_TYPE), 0);

        esp_err_t r = aht_init_desc(&aht, addr, HW_EXTERNAL_PORT, HW_EXTERNAL_SDA_GPIO, HW_EXTERNAL_SCL_GPIO);
        if (r != ESP_OK)
        {
            ESP_LOGW(self->name, "Could not initialize descriptor for device %d: %d (%s)", i, r, esp_err_to_name(r));
            continue;
        }
        if (freq)
            aht.i2c_dev.cfg.master.clk_speed = freq;

        r = aht_init(&aht);
        if (r != ESP_OK)
        {
            ESP_LOGW(self->name, "Could not initialize device %d: %d (%s)", i, r, esp_err_to_name(r));
            aht_free_desc(&aht);
            continue;
        }
        cvector_push_back(sensors, aht);

        device_t dev = { 0 };
        snprintf(dev.uid, sizeof(dev.uid), FMT_HUMIDITY_SENSOR_ID, i);
        dev.type = DEV_SENSOR;
        snprintf(dev.name, sizeof(dev.name), FMT_HUMIDITY_SENSOR_NAME, settings.node.name, i);
        strncpy(dev.device_class, DEV_CLASS_HUMIDITY, sizeof(dev.device_class));
        strncpy(dev.sensor.measurement_unit, DEV_MU_HUMIDITY, sizeof(dev.sensor.measurement_unit));
        dev.sensor.precision = 2;
        dev.sensor.update_period = update_period;
        cvector_push_back(self->devices, dev);

        memset(&dev, 0, sizeof(device_t));
        snprintf(dev.uid, sizeof(dev.uid), FMT_TEMPERATURE_SENSOR_ID, i);
        dev.type = DEV_SENSOR;
        snprintf(dev.name, sizeof(dev.name), FMT_TEMPERATURE_SENSOR_NAME, settings.node.name, i);
        strncpy(dev.device_class, DEV_CLASS_TEMPERATURE, sizeof(dev.device_class));
        strncpy(dev.sensor.measurement_unit, DEV_MU_TEMPERATURE, sizeof(dev.sensor.measurement_unit));
        dev.sensor.precision = 2;
        dev.sensor.update_period = update_period;
        cvector_push_back(self->devices, dev);

        ESP_LOGI(self->name, "Initialized device %d: %s (ADDR=0x%02x, PORT=%d, SDA=%d, SCL=%d, FREQ=%" PRIu32 "kHz)",
            i, aht.type == AHT_TYPE_AHT1x ? "AHT1x" : "AHT20", addr, HW_EXTERNAL_PORT, HW_EXTERNAL_SDA_GPIO, HW_EXTERNAL_SCL_GPIO,
            aht.i2c_dev.cfg.master.clk_speed / 1000);
    }

    return ESP_OK;
}

static void task(driver_t *self)
{
    TickType_t period = pdMS_TO_TICKS(update_period);

    while (true)
    {
        TickType_t start = xTaskGetTickCount();

        for (size_t i = 0; i < cvector_size(sensors); i++)
        {
            float t_sum = 0, rh_sum = 0;
            int real_samples = 0;
            for (int c = 0; c < samples; c++)
            {
                float t = 0, rh = 0;
                esp_err_t r = aht_get_data(&sensors[i], &t, &rh);
                if (r != ESP_OK)
                {
                    ESP_LOGW(self->name, "Error reading device %d: %d (%s)", i, r, esp_err_to_name(r));
                    continue;
                }
                if (t >= (float)threshold)
                {
                    ESP_LOGW(self->name, "Bad temperature value: %.2f >= %d, dropping", t, threshold);
                    continue;
                }
                t_sum += t;
                rh_sum += rh;
                real_samples++;
            }

            device_t *dev = &self->devices[i * 2];
            dev->sensor.value = rh_sum / (float)real_samples;
            driver_send_device_update(self, dev);
            dev = &self->devices[i * 2 + 1];
            dev->sensor.value = t_sum / (float)real_samples;
            driver_send_device_update(self, dev);
        }

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
    for (size_t i = 0; i < cvector_size(sensors); i++)
    {
        esp_err_t r = aht_free_desc(&sensors[i]);
        if (r != ESP_OK)
            ESP_LOGW(self->name, "Device descriptor free error %d: %d (%s)", i, r, esp_err_to_name(r));
    }

    return ESP_OK;
}

driver_t drv_aht = {
    .name = "ahtxx",
    .stack_size = DRIVER_AHTXX_STACK_SIZE,
    .priority = tskIDLE_PRIORITY + 1,
    .defconfig = "{ \"" OPT_PERIOD "\": 5000, \"" OPT_SAMPLES "\": 8, \"" OPT_THRESHOLD "\": 120, \"" OPT_SENSORS "\": " \
        "[{ \"" OPT_TYPE "\": 0, \"" OPT_ADDRESS "\": 56 }] }",

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
