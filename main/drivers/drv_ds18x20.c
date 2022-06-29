#include "drv_ds18x20.h"
#include "common.h"
#include "mqtt.h"
#include "periodic_driver.h"
#include <ds18x20.h>

static ds18x20_addr_t sensors[CONFIG_DS18X20_MAX_SENSORS] = { 0 };
static float results[CONFIG_DS18X20_MAX_SENSORS] = { 0 };
static size_t sensors_count = 0;
static gpio_num_t gpio;
static size_t scan_interval;
static size_t loop_no = 0;

static void loop(driver_t *self)
{
    esp_err_t r;

    if (!(loop_no++ % scan_interval) || !sensors_count)
    {
        r = ds18x20_scan_devices(gpio, sensors, CONFIG_DS18X20_MAX_SENSORS, &sensors_count);
        if (r != ESP_OK)
        {
            ESP_LOGW(self->name, "Error scanning bus: %d (%s)", r, esp_err_to_name(r));
            return;
        }
        if (!sensors_count)
        {
            ESP_LOGW(self->name, "Sensors not found");
            return;
        }
        ESP_LOGI(self->name, "Found %d sensors", sensors_count);
    }

    r = ds18x20_measure_and_read_multi(gpio, sensors, sensors_count, results);
    if (r != ESP_OK)
    {
        ESP_LOGW(self->name, "Error measuring: %d (%s)", r, esp_err_to_name(r));
        return;
    }

    for (size_t i = 0; i < sensors_count; i++)
    {
        char name[32] = { 0 };
        snprintf(name, sizeof(name), "ds18x20_%08x%08x", (uint32_t)(sensors[i] >> 32), (uint32_t)sensors[i]);

        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "family", (sensors[i] & 0xff) == DS18B20_FAMILY_ID ? "DS18B20" : "DS18S20");
        cJSON_AddNumberToObject(json, "temperature", results[i]);
        mqtt_publish_json_subtopic(name, json, 0, 0);
        cJSON_Delete(json);
    }
}

static esp_err_t init(driver_t *self)
{
    self->internal[0] = loop;

    sensors_count = 0;
    loop_no = 0;

    gpio = config_get_gpio(cJSON_GetObjectItem(self->config, "gpio"), GPIO_NUM_NC);
    scan_interval = config_get_int(cJSON_GetObjectItem(self->config, "scan_interval"), 1);

    ESP_LOGI(self->name, "Configured to use GPIO %d with scan_interval %d", gpio, scan_interval);

    esp_err_t r = gpio_reset_pin(gpio);
    if (r != ESP_OK)
    {
        ESP_LOGE(self->name, "Error reset GPIO pin %d: %d (%s)", gpio, r, esp_err_to_name(r));
        return r;
    }

    return periodic_driver_init(self);
}

driver_t drv_ds18x20 = {
    .name = "drv_ds18x20",

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
