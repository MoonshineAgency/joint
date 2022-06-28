#include "drv_aht.h"
#include "common.h"
#include "mqtt.h"
#include "periodic_driver.h"
#include <aht.h>

static aht_t devices[CONFIG_AHT_DRIVER_MAX_DEVICES] = { 0 };
static size_t device_count = 0;

static void loop(driver_t *self)
{
    for (size_t i = 0; i < device_count; i++)
    {
        bool calibrated = false;
        esp_err_t r = aht_get_status(&devices[i], NULL, &calibrated);
        if (r != ESP_OK)
        {
            ESP_LOGW(self->name, "Error reading device %d state: %d (%s)", i, r, esp_err_to_name(r));
            continue;
        }
        float t, rh;
        r = aht_get_data(&devices[i], &t, &rh);
        if (r != ESP_OK)
        {
            ESP_LOGW(self->name, "Error reading device %d: %d (%s)", i, r, esp_err_to_name(r));
            continue;
        }
        char topic[32] = { 0 };
        snprintf(topic, sizeof(topic), "aht_%d_%d_%d_%02x", i, devices[i].i2c_dev.cfg.scl_io_num, devices[i].i2c_dev.cfg.sda_io_num, devices[i].i2c_dev.addr);
        cJSON *msg = cJSON_CreateObject();
        cJSON_AddBoolToObject(msg, "calibrated", calibrated);
        cJSON_AddNumberToObject(msg, "humidity", rh);
        cJSON_AddNumberToObject(msg, "temperature", t);
        mqtt_publish_json_subtopic(topic, msg, 0, 0);
        cJSON_Delete(msg);
    }
}

static esp_err_t init(driver_t *self)
{
    self->internal[0] = loop;

    memset(devices, 0, sizeof(devices));
    device_count = 0;

    cJSON *sensors_j = cJSON_GetObjectItem(self->config, "sensors");
    for (int i = 0; i < cJSON_GetArraySize(sensors_j); i++)
    {
        cJSON *sensor_j = cJSON_GetArrayItem(sensors_j, i);
        gpio_num_t sda = config_get_gpio(cJSON_GetObjectItem(sensor_j, "sda"));
        gpio_num_t scl = config_get_gpio(cJSON_GetObjectItem(sensor_j, "scl"));
        int port = config_get_int(cJSON_GetObjectItem(sensor_j, "port"), 0);
        uint8_t addr = config_get_int(cJSON_GetObjectItem(sensor_j, "address"), AHT_I2C_ADDRESS_GND);
        int freq = config_get_int(cJSON_GetObjectItem(sensor_j, "frequency"), 0);

        aht_t dev = { 0 };
        dev.type = config_get_int(cJSON_GetObjectItem(sensor_j, "type"), 0);

        esp_err_t r = aht_init_desc(&dev, addr, port, sda, scl);
        if (r != ESP_OK)
        {
            ESP_LOGW(self->name, "Could not initialize descriptor for device %d: %d (%s)", i, r, esp_err_to_name(r));
            continue;
        }
        if (freq)
            dev.i2c_dev.cfg.master.clk_speed = freq;

        r = aht_init(&dev);
        if (r != ESP_OK)
        {
            ESP_LOGW(self->name, "Could not initialize device %d: %d (%s)", i, r, esp_err_to_name(r));
            continue;
        }

        ESP_LOGI(self->name, "Initialized device %d: %s (ADDR=0x%02x, PORT=%d, SDA=%d, SCL=%d, FREQ=%d)",
            device_count, dev.type == AHT_TYPE_AHT1x ? "AHT1x" : "AHT20", addr, port, sda, scl, freq);

        devices[device_count++] = dev;
    }

    return periodic_driver_init(self);
}

static esp_err_t stop(driver_t *self)
{
    for (size_t i = 0; i < device_count; i++)
    {
        esp_err_t r = aht_free_desc(&devices[i]);
        if (r != ESP_OK)
            ESP_LOGW(self->name, "Device descriptor free error %d: %d (%s)", i, r, esp_err_to_name(r));
    }

    return periodic_driver_stop(self);
}

driver_t drv_aht = {
    .name = "drv_aht",

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
