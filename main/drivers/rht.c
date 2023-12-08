#include "rht.h"

#ifdef DRIVER_RHT

#include "driver.h"
#include "settings.h"
#include <aht.h>
#include <si7021.h>

#define PROBE_FREQUENCY 100000

#define FMT_HUMIDITY_SENSOR_ID      "rht%d_rh"
#define FMT_TEMPERATURE_SENSOR_ID   "rht%d_t"

#define FMT_HUMIDITY_SENSOR_NAME    "%s humidity %d (%s)"
#define FMT_TEMPERATURE_SENSOR_NAME "%s temperature %d (%s)"

#define OPT_THRESHOLD               "temp_bad_threshold"

typedef enum {
    SENSOR_AHT1X = 0,
    SENSOR_AHT20,
    SENSOR_SI7021,
} sensor_type_t;

static const char *sensor_types[] = {
    [SENSOR_AHT1X] = "AHT10/AHT15",
    [SENSOR_AHT20] = "AHT20",
    [SENSOR_SI7021] = "Si70xx/HTU2xD",
};

typedef struct {
    sensor_type_t type;
    union
    {
        aht_t aht;
        i2c_dev_t i2c_dev;
    } dev;
} handle_t;

static cvector_vector_type(handle_t) sensors = NULL;

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

//    i2c_dev_t probe = { 0 };
//    probe.port = HW_EXTERNAL_PORT;
//    probe.cfg.sda_io_num = HW_EXTERNAL_SDA_GPIO;
//    probe.cfg.scl_io_num = HW_EXTERNAL_SCL_GPIO;
//    probe.cfg.master.clk_speed = PROBE_FREQUENCY;

    // Init devices
    cJSON *sensors_j = cJSON_GetObjectItem(self->config, OPT_SENSORS);
    for (int i = 0; i < cJSON_GetArraySize(sensors_j); i++)
    {
        cJSON *sensor_j = cJSON_GetArrayItem(sensors_j, i);
        uint8_t addr = driver_config_get_int(cJSON_GetObjectItem(sensor_j, OPT_ADDRESS), AHT_I2C_ADDRESS_GND);
        int freq = driver_config_get_int(cJSON_GetObjectItem(sensor_j, OPT_FREQ), 0);
        int sensor_type = driver_config_get_int(cJSON_GetObjectItem(sensor_j, OPT_TYPE), 0);

//        esp_err_t r = i2c_dev_probe(&probe, I2C_DEV_WRITE);
//        if (r != ESP_OK)
//        {
//            ESP_LOGW(self->name, "I2C device with address = 0x%02x not found %d (%s)", addr, r, esp_err_to_name(r));
//            continue;
//        }

        esp_err_t r;
        handle_t sensor;
        memset(&sensor, 0, sizeof(handle_t));
        switch (sensor_type)
        {
            case SENSOR_AHT1X:
            case SENSOR_AHT20:
                sensor.dev.aht.type = sensor_type;
                r = aht_init_desc(&sensor.dev.aht, addr, HW_EXTERNAL_PORT, HW_EXTERNAL_SDA_GPIO, HW_EXTERNAL_SCL_GPIO);
                if (r != ESP_OK)
                {
                    ESP_LOGW(self->name, "Could not initialize descriptor for AHTxx %d: %d (%s)", i, r, esp_err_to_name(r));
                    continue;
                }
                if (freq)
                    sensor.dev.aht.i2c_dev.cfg.master.clk_speed = freq;
                r = aht_init(&sensor.dev.aht);
                if (r != ESP_OK)
                {
                    ESP_LOGW(self->name, "Could not initialize device %d: %d (%s)", i, r, esp_err_to_name(r));
                    aht_free_desc(&sensor.dev.aht);
                    continue;
                }
                break;
            case SENSOR_SI7021:
                r = si7021_init_desc(&sensor.dev.i2c_dev, HW_EXTERNAL_PORT, HW_EXTERNAL_SDA_GPIO, HW_EXTERNAL_SCL_GPIO);
                if (r != ESP_OK)
                {
                    ESP_LOGW(self->name, "Could not initialize descriptor for Si70xx/HTU2xD %d: %d (%s)", i, r, esp_err_to_name(r));
                    continue;
                }
                if (freq)
                    sensor.dev.i2c_dev.cfg.master.clk_speed = freq;
                break;
            default:
                ESP_LOGW(self->name, "Unknown sensor type %d", sensor_type);
                continue;
        }
        sensor.type = sensor_type;

        cvector_push_back(sensors, sensor);

        device_t dev = { 0 };
        snprintf(dev.uid, sizeof(dev.uid), FMT_HUMIDITY_SENSOR_ID, i);
        dev.type = DEV_SENSOR;
        snprintf(dev.name, sizeof(dev.name), FMT_HUMIDITY_SENSOR_NAME, settings.system.name, i, sensor_types[sensor_type]);
        strncpy(dev.device_class, DEV_CLASS_HUMIDITY, sizeof(dev.device_class));
        strncpy(dev.sensor.measurement_unit, DEV_MU_HUMIDITY, sizeof(dev.sensor.measurement_unit));
        dev.sensor.precision = 2;
        dev.sensor.update_period = update_period;
        cvector_push_back(self->devices, dev);

        memset(&dev, 0, sizeof(device_t));
        snprintf(dev.uid, sizeof(dev.uid), FMT_TEMPERATURE_SENSOR_ID, i);
        dev.type = DEV_SENSOR;
        snprintf(dev.name, sizeof(dev.name), FMT_TEMPERATURE_SENSOR_NAME, settings.system.name, i, sensor_types[sensor_type]);
        strncpy(dev.device_class, DEV_CLASS_TEMPERATURE, sizeof(dev.device_class));
        strncpy(dev.sensor.measurement_unit, DEV_MU_TEMPERATURE, sizeof(dev.sensor.measurement_unit));
        dev.sensor.precision = 2;
        dev.sensor.update_period = update_period;
        cvector_push_back(self->devices, dev);

        ESP_LOGI(self->name, "Initialized device %d: %s (ADDR=0x%02x, PORT=%d, SDA=%d, SCL=%d)",
            i, sensor_types[sensor_type], addr, HW_EXTERNAL_PORT, HW_EXTERNAL_SDA_GPIO, HW_EXTERNAL_SCL_GPIO);
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
                esp_err_t r;

                if (sensors[i].type == SENSOR_SI7021)
                {
                    r = si7021_measure_temperature(&sensors[i].dev.i2c_dev, &t);
                    if (r != ESP_OK)
                    {
                        ESP_LOGW(self->name, "Error measuring temperature with Si70xx/HTU2xD device %d: %d (%s)", i, r, esp_err_to_name(r));
                        continue;
                    }
                    r = si7021_measure_humidity(&sensors[i].dev.i2c_dev, &rh);
                    if (r != ESP_OK)
                    {
                        ESP_LOGW(self->name, "Error measuring humidity with Si70xx/HTU2xD device %d: %d (%s)", i, r, esp_err_to_name(r));
                        continue;
                    }
                }
                else
                {
                    r = aht_get_data(&sensors[i].dev.aht, &t, &rh);
                    if (r != ESP_OK)
                    {
                        ESP_LOGW(self->name, "Error reading AHTxx device %d: %d (%s)", i, r, esp_err_to_name(r));
                        continue;
                    }
                }
                if (t >= (float)threshold)
                {
                    ESP_LOGW(self->name, "Bad temperature value: %.2f >= %d, dropping", t, threshold);
                    continue;
                }
                t_sum += t;
                rh_sum += rh;
                real_samples++;
                vTaskDelay(1);
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
        esp_err_t r;
        if (sensors[i].type == SENSOR_SI7021)
            r = si7021_free_desc(&sensors[i].dev.i2c_dev);
        else
            r = aht_free_desc(&sensors[i].dev.aht);
        if (r != ESP_OK)
            ESP_LOGW(self->name, "Device descriptor free error %d: %d (%s)", i, r, esp_err_to_name(r));
    }

    return ESP_OK;
}

driver_t drv_rht = {
    .name = "rht",
    .stack_size = DRIVER_RHT_STACK_SIZE,
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

#endif
