#include "drv_ds18x20.h"
#include <esp_log.h>
#include <ds18x20.h>
#include "settings.h"

#define SENSOR_ADDR_FMT "%08X%08X"
#define SENSOR_ADDR(addr) (uint32_t)(addr >> 32), (uint32_t)addr

static gpio_num_t gpio;
static size_t scan_interval;

static ds18x20_addr_t sensors[CONFIG_DS18X20_MAX_SENSORS] = { 0 };
static float results[CONFIG_DS18X20_MAX_SENSORS] = { 0 };

static size_t sensors_count = 0;
static size_t loop_no = 0;

static esp_err_t on_init(driver_t *self)
{
    cvector_free(self->devices);
    memset(sensors, 0, sizeof(sensors));
    sensors_count = 0;
    loop_no = 0;

    gpio = driver_config_get_gpio(cJSON_GetObjectItem(self->config, "gpio"), GPIO_NUM_NC);
    scan_interval = driver_config_get_int(cJSON_GetObjectItem(self->config, "scan_interval"), 1);

    ESP_LOGI(self->name, "Configured to use GPIO %d with scan_interval %d", gpio, scan_interval);

    esp_err_t r = gpio_reset_pin(gpio);
    if (r != ESP_OK)
        ESP_LOGE(self->name, "Error reset GPIO pin %d: %d (%s)", gpio, r, esp_err_to_name(r));

    return r;
}

static void scan(driver_t *self)
{
    esp_err_t r;

    size_t result_count = 0;
    ds18x20_addr_t scan_result[CONFIG_DS18X20_MAX_SENSORS] = {0 };

    r = ds18x20_scan_devices(gpio, scan_result, CONFIG_DS18X20_MAX_SENSORS, &result_count);
    if (r != ESP_OK)
    {
        ESP_LOGW(self->name, "Error scanning bus: %d (%s)", r, esp_err_to_name(r));
        return;
    }
    if (!result_count)
    {
        ESP_LOGW(self->name, "Sensors not found");
        return;
    }

    if (memcmp(sensors, scan_result, sizeof(sensors)) == 0)
        return;

    // something changed
    // 1. remove disconnected
    for (size_t i = 0; i < sensors_count; i++)
    {
        bool found = false;
        for (size_t c = 0; c < result_count; c++)
            if (sensors[i] == scan_result[c])
            {
                found = true;
                break;
            }
        if (!found)
        {
            ESP_LOGI(self->name, "Removed " SENSOR_ADDR_FMT, SENSOR_ADDR(sensors[i]));
            driver_send_device_remove(self, &self->devices[i]);
        }
    }
    // 2. recreate devices
    cvector_free(self->devices);
    for (size_t i = 0; i < result_count; i++)
    {
        device_t dev = {0};
        dev.type = DEV_SENSOR;
        snprintf(dev.uid, sizeof(dev.uid), SENSOR_ADDR_FMT, SENSOR_ADDR(scan_result[i]));
        snprintf(dev.name, sizeof(dev.name), "%s temperature (DS18x20 " SENSOR_ADDR_FMT ")", settings.node.name, SENSOR_ADDR(scan_result[i]));
        strncpy(dev.sensor.measurement_unit, "°C", sizeof(dev.sensor.measurement_unit));
        dev.sensor.precision = 2;
        cvector_push_back(self->devices, dev);
    }
    // 3. add connected
    for (size_t i = 0; i < result_count; i++)
    {
        bool found = false;
        for (size_t c = 0; c < sensors_count; c++)
            if (scan_result[i] == sensors[c])
            {
                found = true;
                break;
            }
        if (!found)
        {
            ESP_LOGI(self->name, "Added " SENSOR_ADDR_FMT, SENSOR_ADDR(scan_result[i]));
            driver_send_device_add(self, &self->devices[i]);
        }
    }
    // copy
    memcpy(sensors, scan_result, sizeof(sensors));
    sensors_count = result_count;
}

static void task(driver_t *self)
{
    TickType_t period = pdMS_TO_TICKS(driver_config_get_int(cJSON_GetObjectItem(self->config, "period"), 1000));

    while (true)
    {
        TickType_t start = xTaskGetTickCount();

        if (!(loop_no++ % scan_interval) || !sensors_count)
            scan(self);

        esp_err_t r = ds18x20_measure_and_read_multi(gpio, sensors, sensors_count, results);
        if (r == ESP_OK)
            for (size_t i = 0; i < sensors_count; i++)
            {
                self->devices[i].sensor.value = results[i];
                driver_send_device_update(self, &self->devices[i]);
            }
        else
            ESP_LOGW(self->name, "Error measuring: %d (%s)", r, esp_err_to_name(r));

        while (xTaskGetTickCount() - start < period)
        {
            if (!(xEventGroupGetBits(self->eg) & DRIVER_BIT_START))
                return;
            vTaskDelay(1);
        }
    }
}

driver_t drv_ds18x20 = {
    .name = "drv_ds18x20",
    .defconfig = "{ \"stack_size\": 4096, \"gpio\": 15, \"scan_interval\": 10 }",

    .config = NULL,
    .state = DRIVER_NEW,
    .event_queue = NULL,

    .devices = NULL,
    .handle = NULL,
    .eg = NULL,

    .on_init = on_init,
    .on_start = NULL,
    .on_stop = NULL,

    .task = task
};
