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
#define AIN_COUNT 4
#define TDS_CHANNEL ADC_CHANNEL_3

static adc_oneshot_unit_handle_t adc_handle = NULL;
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
static bool moisture_enabled;

static adc_cali_handle_t adc_cal_handle = NULL;
static calibration_handle_t moisture_calib = { 0 };
static const calibration_point_t def_moisture_calib[]= {
    { .code = 2.2f, .value = 0.0f },
    { .code = 0.9f, .value = 100.0f, },
};
static const size_t def_moisture_calib_points = sizeof(def_moisture_calib) / sizeof(calibration_point_t);

#ifdef DRIVER_GH_ADC_TDS_ENABLE
static adc_cali_handle_t tds_cal_handle = NULL;
static calibration_handle_t tds_calib = { 0 };
static const calibration_point_t def_tds_calib[]= {
    { .code = 0.0f, .value = 0.0f },
    { .code = 1.0f, .value = 1.0f, },
};
static const size_t def_tds_calib_points = sizeof(def_tds_calib) / sizeof(calibration_point_t);
#endif

static esp_err_t create_adc_cali_scheme(adc_atten_t atten, adc_cali_handle_t *cal_handle)
{
    adc_cali_line_fitting_efuse_val_t efuse_cal = ADC_CALI_LINE_FITTING_EFUSE_VAL_EFUSE_VREF;
    adc_cali_scheme_line_fitting_check_efuse(&efuse_cal);

    adc_cali_line_fitting_config_t cal_cfg = {
        .atten = atten,
        .bitwidth = ADC_WIDTH,
        .unit_id = unit_cfg.unit_id,
        .default_vref = efuse_cal == ADC_CALI_LINE_FITTING_EFUSE_VAL_DEFAULT_VREF ? 1100 : 0,
    };
    return adc_cali_create_scheme_line_fitting(&cal_cfg, cal_handle);
}

static esp_err_t on_init(driver_t *self)
{
    cvector_free(self->devices);
    if (adc_handle)
    {
        adc_oneshot_del_unit(adc_handle);
        adc_handle = NULL;
    }
    if (adc_cal_handle)
    {
        adc_cali_delete_scheme_line_fitting(adc_cal_handle);
        adc_cal_handle = NULL;
    }
#ifdef DRIVER_GH_ADC_TDS_ENABLE
    if (tds_cal_handle)
    {
        adc_cali_delete_scheme_line_fitting(tds_cal_handle);
        tds_cal_handle = NULL;
    }
#endif
    adc_atten_t atten = driver_config_get_int(cJSON_GetObjectItem(self->config, OPT_ATTEN), ADC_ATTEN_DB_11);
    samples = driver_config_get_int(cJSON_GetObjectItem(self->config, OPT_SAMPLES), 64);
    update_period = driver_config_get_int(cJSON_GetObjectItem(self->config, OPT_PERIOD), 1000);
    moisture_enabled = driver_config_get_bool(cJSON_GetObjectItem(self->config, OPT_MOISTURE), false);

    if (moisture_enabled)
        CHECK(driver_config_read_calibration(self, cJSON_GetObjectItem(self->config, OPT_MOISTURE_CALIBRATION),
            OPT_VOLTAGE, OPT_MOISTURE, &moisture_calib, def_moisture_calib, def_moisture_calib_points));

#ifdef DRIVER_GH_ADC_TDS_ENABLE
    CHECK(driver_config_read_calibration(self, cJSON_GetObjectItem(self->config, OPT_TDS_CALIBRATION),
        OPT_VOLTAGE, OPT_TDS, &tds_calib, def_tds_calib, def_tds_calib_points));
#endif

    ESP_RETURN_ON_ERROR(
        adc_oneshot_new_unit(&unit_cfg, &adc_handle),
        self->name, "Error initializing ADC UNIT 1: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );

    // configure four ADC channels
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = atten,
        .bitwidth = ADC_WIDTH,
    };
    for (size_t c = 0; c < AIN_COUNT; c++)
        ESP_RETURN_ON_ERROR(
            adc_oneshot_config_channel(adc_handle, ain_channels[c], &chan_cfg),
            self->name, "Error configuring ADC channel: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
        );
    // ADC calibration
    ESP_RETURN_ON_ERROR(
        create_adc_cali_scheme(atten, &adc_cal_handle),
        self->name, "Error creating ADC calibration scheme: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );

#ifdef DRIVER_GH_ADC_TDS_ENABLE
    // configure TDS measure channel
    chan_cfg.atten = DRIVER_GH_ADC_TDS_ATTEN;
    ESP_RETURN_ON_ERROR(
        adc_oneshot_config_channel(adc_handle, TDS_CHANNEL, &chan_cfg),
        self->name, "Error configuring ADC channel: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );
    // TDS calibration
    ESP_RETURN_ON_ERROR(
        create_adc_cali_scheme(DRIVER_GH_ADC_TDS_ATTEN, &tds_cal_handle),
        self->name, "Error creating TDS calibration scheme: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );
#endif

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

    if (moisture_enabled)
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

#ifdef DRIVER_GH_ADC_TDS_ENABLE
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
#endif

    return ESP_OK;
}

static int adc_read_voltage(driver_t *self, adc_channel_t channel, adc_cali_handle_t cal_handle)
{
    int raw, res;
    esp_err_t r = adc_oneshot_read(adc_handle, channel, &raw);
    if (r != ESP_OK)
    {
        ESP_LOGE(self->name, "Error reading ADC1 channel %d: %d (%s)", channel, r, esp_err_to_name(r));
        return 0;
    }
    r = adc_cali_raw_to_voltage(cal_handle, raw, &res);
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
#ifdef DRIVER_GH_ADC_TDS_ENABLE
        tds_voltage = 0;
#endif

        // Read raw ADC values
        for (size_t i = 0; i < samples; i++)
        {
            for (size_t c = 0; c < AIN_COUNT; c++)
                ain_voltages[c] += adc_read_voltage(self, ain_channels[c], adc_cal_handle);

#ifdef DRIVER_GH_ADC_TDS_ENABLE
            tds_voltage += adc_read_voltage(self, TDS_CHANNEL, tds_cal_handle);
#endif
        }

        // Write raw ADC values
        for (size_t c = 0; c < AIN_COUNT; c++)
        {
            float voltage = (float)ain_voltages[c] / (float)samples / 1000.0f;
            self->devices[c].sensor.value = voltage;
            driver_send_device_update(self, &self->devices[c]);
        }

        device_t *dev;

        // Calculate and write moisture values
        if (moisture_enabled)
            for (size_t c = 0; c < AIN_COUNT; c++)
            {
                float moisture;
                r = calibration_get_value(&moisture_calib, self->devices[c].sensor.value, &moisture);
                if (r != ESP_OK)
                {
                    ESP_LOGE(self->name, "Error getting calibrated moisture value: %d (%s)", r, esp_err_to_name(r));
                    goto next;
                }
                dev = &self->devices[c + AIN_COUNT];
                dev->sensor.value = moisture;
                driver_send_device_update(self, dev);
            }

#ifdef DRIVER_GH_ADC_TDS_ENABLE
        // Write raw TDS voltage
        float tds_raw = (float)tds_voltage / (float)samples / 1000.0f;
        dev = &self->devices[cvector_size(self->devices) - 2];
        dev->sensor.value = tds_raw;
        driver_send_device_update(self, dev);

        // Calculate and write TDS
        float tds;
        r = calibration_get_value(&tds_calib, tds_raw, &tds);
        if (r != ESP_OK)
        {
            ESP_LOGE(self->name, "Error getting calibrated TDS value: %d (%s)", r, esp_err_to_name(r));
            goto next;
        }
        dev = &self->devices[cvector_size(self->devices) - 1];
        dev->sensor.value = tds;
        driver_send_device_update(self, dev);
#endif

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
#ifdef DRIVER_GH_ADC_TDS_ENABLE
    r = calibration_free(&tds_calib);
    if (r != ESP_OK)
        ESP_LOGW(self->name, "TDS calibration data free error: %d (%s)", r, esp_err_to_name(r));
#endif

    return ESP_OK;
}

driver_t drv_gh_adc = {
    .name = "gh_adc",
    .stack_size = DRIVER_GH_ADC_STACK_SIZE,
    .priority = tskIDLE_PRIORITY + 1,
#ifdef DRIVER_GH_ADC_TDS_ENABLE
    .defconfig = "{ \"" OPT_PERIOD "\": 2000, \"" OPT_SAMPLES "\": 64, \"" OPT_ATTEN "\": 3, " \
        "\"" OPT_MOISTURE "\": true, " \
        "\"" OPT_MOISTURE_CALIBRATION "\": [{\"" OPT_VOLTAGE "\": 2.2, \"" OPT_MOISTURE "\": 0}, " \
        "{\"" OPT_VOLTAGE "\": 0.9, \"" OPT_MOISTURE "\": 100}], " \
        "\"" OPT_TDS_CALIBRATION "\": " \
        "[{\"" OPT_VOLTAGE "\": 0, \"" OPT_TDS "\": 0}, {\"" OPT_VOLTAGE "\": 1, \"" OPT_TDS "\": 1}]}",
#else
    .defconfig = "{ \"" OPT_PERIOD "\": 2000, \"" OPT_SAMPLES "\": 64, \"" OPT_ATTEN "\": 3, " \
        "\"" OPT_MOISTURE "\": true, " \
        "\"" OPT_MOISTURE_CALIBRATION "\": [{\"" OPT_VOLTAGE "\": 2.2, \"" OPT_MOISTURE "\": 0}, " \
        "{\"" OPT_VOLTAGE "\": 0.9, \"" OPT_MOISTURE "\": 100}]}",
#endif

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
