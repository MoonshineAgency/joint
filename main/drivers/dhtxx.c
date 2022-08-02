#include "dhtxx.h"
#include "common.h"
#include "driver.h"
#include "settings.h"
#include <dht.h>

typedef struct
{
    gpio_num_t gpio;
    dht_sensor_type_t type;
} sensor_t;

static cvector_vector_type(sensor_t) sensors = NULL;

static int update_period;

static esp_err_t on_init(driver_t *self)
{
    cvector_free(self->devices);
    cvector_free(sensors);

    update_period = driver_config_get_int(cJSON_GetObjectItem(self->config, "period"), 1000);

    // Init devices
    cJSON *sensors_j = cJSON_GetObjectItem(self->config, "sensors");
    for (int i = 0; i < cJSON_GetArraySize(sensors_j); i++)
    {
        cJSON *sensor_j = cJSON_GetArrayItem(sensors_j, i);
        sensor_t s;
        s.gpio = driver_config_get_gpio(cJSON_GetObjectItem(sensor_j, "gpio"), GPIO_NUM_NC);
        if (s.gpio == GPIO_NUM_NC)
        {
            ESP_LOGW(self->name, "Invalid GPIO number");
            continue;
        }
        s.type = driver_config_get_int(cJSON_GetObjectItem(sensor_j, "type"), DHT_TYPE_DHT11);
        if (s.type > DHT_TYPE_SI7021)
        {
            ESP_LOGW(self->name, "Invalid DHT sensor type %d", s.type);
            continue;
        }
        bool pullup = driver_config_get_gpio(cJSON_GetObjectItem(sensor_j, "pullup"), false);
        gpio_reset_pin(s.gpio);
        if (pullup)
            gpio_set_pull_mode(s.gpio, GPIO_PULLUP_ONLY);

        cvector_push_back(sensors, s);

        device_t dev = { 0 };
        snprintf(dev.uid, sizeof(dev.uid), "dht_rh%d", i);
        dev.type = DEV_SENSOR;
        snprintf(dev.name, sizeof(dev.name), "%s humidity (DHT sensor %d)", settings.node.name, i);
        strncpy(dev.device_class, "humidity", sizeof(dev.device_class));
        strncpy(dev.sensor.measurement_unit, "%", sizeof(dev.sensor.measurement_unit));
        dev.sensor.precision = 1;
        dev.sensor.update_period = update_period;
        cvector_push_back(self->devices, dev);

        memset(&dev, 0, sizeof(device_t));
        snprintf(dev.uid, sizeof(dev.uid), "dht_t%d", i);
        dev.type = DEV_SENSOR;
        snprintf(dev.name, sizeof(dev.name), "%s temperature (DHT sensor %d)", settings.node.name, i);
        strncpy(dev.device_class, "temperature", sizeof(dev.device_class));
        strncpy(dev.sensor.measurement_unit, "°C", sizeof(dev.sensor.measurement_unit));
        dev.sensor.precision = 1;
        dev.sensor.update_period = update_period;
        cvector_push_back(self->devices, dev);
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
            float t = 0, rh = 0;
            esp_err_t r = dht_read_float_data(sensors[i].type, sensors[i].gpio, &rh, &t);
            if (r != ESP_OK)
            {
                ESP_LOGW(self->name, "Error reading device %d: %d (%s)", i, r, esp_err_to_name(r));
                continue;
            }

            device_t *dev = &self->devices[i * 2];
            dev->sensor.value = rh;
            driver_send_device_update(self, dev);
            dev = &self->devices[i * 2 + 1];
            dev->sensor.value = t;
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

driver_t drv_dht = {
    .name = "dhtxx",
    .defconfig = "{ \"stack_size\": 4096, \"period\": 5000, \"sensors\": [{ \"gpio\": 21, \"type\": 0, \"pullup\": true }] }",

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
