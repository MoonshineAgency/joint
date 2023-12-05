#include "gh_adc.h"

#ifdef DRIVER_GH_ADC

#include <esp_log.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali_scheme.h>
#include <calibration.h>
#include "settings.h"

#define FMT_ADC_SENSOR_ID        "ain%d"
#define FMT_MOISTURE_SENSOR_ID   "ain%d_moisture"
#define FMT_TDS_RAW_SENSOR_ID    "tds0_raw"
#define FMT_TDS_SENSOR_ID        "tds0"

#define FMT_ADC_SENSOR_NAME      "%s analog input %d"
#define FMT_MOISTURE_SENSOR_NAME "%s moisture sensor on AIN%d"
#define FMT_TDS_RAW_SENSOR_NAME  "%s TDS input 0 raw voltage"
#define FMT_TDS_SENSOR_NAME      "%s TDS input 0"

#define ADC_WIDTH ADC_BITWIDTH_12
#define TDS_ATTEN ADC_ATTEN_DB_6 // 1.75V max
#define AIN_COUNT 4
#define TDS_RAW_DEV_IDX (AIN_COUNT * 2)
#define TDS_CHANNEL ADC_CHANNEL_3

static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t cali_handle = NULL;

static const adc_oneshot_unit_init_cfg_t unit_cfg = {
    .unit_id = ADC_UNIT_1,
    .ulp_mode = ADC_ULP_MODE_DISABLE,
};

static const adc_channel_t ain_channels[AIN_COUNT] = {
    ADC_CHANNEL_4,
    ADC_CHANNEL_5,
    ADC_CHANNEL_6,
    ADC_CHANNEL_7,
};

static size_t samples;
static int update_period;

static calibration_handle_t moisture_calib = { 0 };
static calibration_handle_t tds_calib = { 0 };

static const calibration_point_t def_moisture_calib[]= {
    { .code = 2.2f, .value = 0.0f },
    { .code = 0.9f, .value = 100.0f, },
};
static const size_t def_moisture_calib_points = sizeof(def_moisture_calib) / sizeof(calibration_point_t);

static const calibration_point_t def_tds_calib[]= {
    { .code = 0.0f, .value = 0.0f },
    { .code = 1.0f, .value = 1.0f, },
};
static const size_t def_tds_calib_points = sizeof(def_tds_calib) / sizeof(calibration_point_t);

static esp_err_t on_init(driver_t *self)
{
    cvector_free(self->devices);
    if (adc_handle)
    {
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
    }
    if (cali_handle)
    {
        adc_cali_delete_scheme_line_fitting(cali_handle);
        cali_handle = NULL;
    }

    adc_atten_t atten = driver_config_get_int(cJSON_GetObjectItem(self->config, OPT_ATTEN), ADC_ATTEN_DB_11);
    samples = driver_config_get_int(cJSON_GetObjectItem(self->config, OPT_SAMPLES), 64);
    update_period = driver_config_get_int(cJSON_GetObjectItem(self->config, OPT_PERIOD), 1000);

    CHECK(driver_config_read_calibration(self, cJSON_GetObjectItem(self->config, OPT_MOISTURE_CALIBRATION),
        OPT_VOLTAGE, OPT_MOISTURE, &moisture_calib, def_moisture_calib, def_moisture_calib_points));
    CHECK(driver_config_read_calibration(self, cJSON_GetObjectItem(self->config, OPT_TDS_CALIBRATION),
        OPT_VOLTAGE, OPT_TDS, &tds_calib, def_tds_calib, def_tds_calib_points));

    ESP_RETURN_ON_ERROR(
        adc_oneshot_new_unit(&unit_cfg, &adc_handle),
        self->name, "Error initializing ADC UNIT 1: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );

    // four free to use ADC channels
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = atten,
        .bitwidth = ADC_WIDTH,
    };
    for (size_t c = 0; c < AIN_COUNT; c++)
        ESP_RETURN_ON_ERROR(
            adc_oneshot_config_channel(adc_handle, ain_channels[c], &chan_cfg),
            self->name, "Error configuring ADC channel: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
        );

    // TDS measure channel
    chan_cfg.atten = TDS_ATTEN;
    ESP_RETURN_ON_ERROR(
        adc_oneshot_config_channel(adc_handle, TDS_CHANNEL, &chan_cfg),
        self->name, "Error configuring ADC channel: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );

    // calibration
    adc_cali_line_fitting_efuse_val_t efuse_cali = ADC_CALI_LINE_FITTING_EFUSE_VAL_EFUSE_VREF;
    adc_cali_scheme_line_fitting_check_efuse(&efuse_cali);

    adc_cali_line_fitting_config_t cail_cfg = {
        .atten = atten,
        .bitwidth = ADC_WIDTH,
        .unit_id = unit_cfg.unit_id,
        .default_vref = efuse_cali == ADC_CALI_LINE_FITTING_EFUSE_VAL_DEFAULT_VREF ? 1100 : 0,
    };
    ESP_RETURN_ON_ERROR(
        adc_cali_create_scheme_line_fitting(&cail_cfg, &cali_handle),
        self->name, "Error creating ADC calibration scheme: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );

    device_t dev = { 0 };
    for (size_t c = 0; c < AIN_COUNT; c++)
    {
        memset(&dev, 0, sizeof(dev));
        dev.type = DEV_SENSOR;
        dev.sensor.precision = 3;
        dev.sensor.update_period = update_period;
        strncpy(dev.sensor.measurement_unit, DEV_MU_VOLTAGE, sizeof(dev.sensor.measurement_unit));
        strncpy(dev.device_class, DEV_CLASS_VOLTAGE, sizeof(dev.device_class));
        snprintf(dev.uid, sizeof(dev.uid), FMT_ADC_SENSOR_ID, c);
        snprintf(dev.name, sizeof(dev.name), FMT_ADC_SENSOR_NAME, settings.system.name, c);
        cvector_push_back(self->devices, dev);
    }

    for (size_t c = 0; c < AIN_COUNT; c++)
    {
        memset(&dev, 0, sizeof(dev));
        dev.type = DEV_SENSOR;
        dev.sensor.precision = 1;
        dev.sensor.update_period = update_period;
        strncpy(dev.sensor.measurement_unit, DEV_MU_MOISTURE, sizeof(dev.sensor.measurement_unit));
        strncpy(dev.device_class, DEV_CLASS_MOISTURE, sizeof(dev.device_class));
        snprintf(dev.uid, sizeof(dev.uid), FMT_MOISTURE_SENSOR_ID, c);
        snprintf(dev.name, sizeof(dev.name), FMT_MOISTURE_SENSOR_NAME, settings.system.name, c);
        cvector_push_back(self->devices, dev);
    }

    memset(&dev, 0, sizeof(dev));
    dev.type = DEV_SENSOR;
    dev.sensor.precision = 3;
    dev.sensor.update_period = update_period;
    strncpy(dev.sensor.measurement_unit, DEV_MU_VOLTAGE, sizeof(dev.sensor.measurement_unit));
    strncpy(dev.device_class, DEV_CLASS_VOLTAGE, sizeof(dev.device_class));
    strncpy(dev.uid, FMT_TDS_RAW_SENSOR_ID, sizeof(dev.uid));
    snprintf(dev.name, sizeof(dev.name), FMT_TDS_RAW_SENSOR_NAME, settings.system.name);
    cvector_push_back(self->devices, dev);

    memset(&dev, 0, sizeof(dev));
    dev.type = DEV_SENSOR;
    dev.sensor.precision = 1;
    dev.sensor.update_period = update_period;
    strncpy(dev.sensor.measurement_unit, DEV_MU_TDS, sizeof(dev.sensor.measurement_unit));
    strncpy(dev.uid, FMT_TDS_SENSOR_ID, sizeof(dev.uid));
    snprintf(dev.name, sizeof(dev.name), FMT_TDS_SENSOR_NAME, settings.system.name);
    cvector_push_back(self->devices, dev);

    return ESP_OK;
}

static int adc_read_voltage(driver_t *self, adc_channel_t channel)
{
    int raw, res;
    esp_err_t r = adc_oneshot_read(adc_handle, channel, &raw);
    if (r != ESP_OK)
    {
        ESP_LOGE(self->name, "Error reading ADC1 channel %d: %d (%s)", channel, r, esp_err_to_name(r));
        return 0;
    }
    r = adc_cali_raw_to_voltage(cali_handle, raw, &res);
    if (r != ESP_OK)
    {
        ESP_LOGE(self->name, "Error converting raw ADC value to voltage: %d (%s)", r, esp_err_to_name(r));
        return 0;
    }

    return res;
}

static void task(driver_t *self)
{
    int ain_voltages[AIN_COUNT];
    int tds_voltage;
    esp_err_t r;

    TickType_t period = pdMS_TO_TICKS(update_period);

    while (true)
    {
        TickType_t start = xTaskGetTickCount();

        memset(ain_voltages, 0, sizeof(ain_voltages));
        tds_voltage = 0;

        // Read raw ADC values
        for (size_t i = 0; i < samples; i++)
        {
            for (size_t c = 0; c < AIN_COUNT; c++)
                ain_voltages[c] += adc_read_voltage(self, ain_channels[c]);

            tds_voltage += adc_read_voltage(self, TDS_CHANNEL);
        }

        // Write raw ADC values
        for (size_t c = 0; c < AIN_COUNT; c++)
        {
            float voltage = (float)ain_voltages[c] / (float)samples / 1000.0f;
            self->devices[c].sensor.value = voltage;
            driver_send_device_update(self, &self->devices[c]);
        }

        // Calculate and write moisture values
        for (size_t c = 0; c < AIN_COUNT; c++)
        {
            float moisture;
            r = calibration_get_value(&moisture_calib, self->devices[c].sensor.value, &moisture);
            if (r != ESP_OK)
            {
                ESP_LOGE(self->name, "Error getting calibrated moisture value: %d (%s)", r, esp_err_to_name(r));
                goto next;
            }
            self->devices[c + AIN_COUNT].sensor.value = moisture;
            driver_send_device_update(self, &self->devices[c + AIN_COUNT]);
        }

        // Write raw TDS voltage
        self->devices[TDS_RAW_DEV_IDX].sensor.value = (float)tds_voltage / (float)samples / 1000.0f;
        driver_send_device_update(self, &self->devices[TDS_RAW_DEV_IDX]);

        // Calculate and write TDS
        float tds;
        r = calibration_get_value(&tds_calib, self->devices[TDS_RAW_DEV_IDX].sensor.value, &tds);
        if (r != ESP_OK)
        {
            ESP_LOGE(self->name, "Error getting calibrated TDS value: %d (%s)", r, esp_err_to_name(r));
            goto next;
        }
        self->devices[TDS_RAW_DEV_IDX + 1].sensor.value = tds;
        driver_send_device_update(self, &self->devices[TDS_RAW_DEV_IDX + 1]);

    next:
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
    esp_err_t r = calibration_free(&moisture_calib);
    if (r != ESP_OK)
        ESP_LOGW(self->name, "Moisture calibration data free error: %d (%s)", r, esp_err_to_name(r));
    r = calibration_free(&tds_calib);
    if (r != ESP_OK)
        ESP_LOGW(self->name, "TDS calibration data free error: %d (%s)", r, esp_err_to_name(r));

    return ESP_OK;
}

driver_t drv_gh_adc = {
    .name = "gh_adc",
    .stack_size = DRIVER_GH_ADC_STACK_SIZE,
    .priority = tskIDLE_PRIORITY + 1,
    .defconfig = "{ \"" OPT_PERIOD "\": 2000, \"" OPT_SAMPLES "\": 64, \"" OPT_ATTEN "\": 3, " \
        "\"" OPT_MOISTURE_CALIBRATION "\": [{\"" OPT_VOLTAGE "\": 2.2, \"" OPT_MOISTURE "\": 0}, " \
        "{\"" OPT_VOLTAGE "\": 0.9, \"" OPT_MOISTURE "\": 100}], \"" OPT_TDS_CALIBRATION "\": " \
        "[{\"" OPT_VOLTAGE "\": 0, \"" OPT_TDS "\": 0}, {\"" OPT_VOLTAGE "\": 1, \"" OPT_TDS "\": 1}]}",

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
