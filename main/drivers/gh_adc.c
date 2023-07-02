#include "gh_adc.h"

#ifdef DRIVER_GH_ADC

#include <esp_log.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali_scheme.h>
#include "settings.h"

#define FMT_ADC_SENSOR_ID        "ain%d"
#define FMT_MOISTURE_SENSOR_ID   "ain_moisture%d"
#define FMT_TDS_SENSOR_ID        "tds0"

#define FMT_ADC_SENSOR_NAME      "%s analog input %d"
#define FMT_MOISTURE_SENSOR_NAME "%s moisture sensor on AIN%d"
#define FMT_TDS_SENSOR_NAME      "%s TDS input 0 raw voltage"

#define ADC_WIDTH ADC_BITWIDTH_12
#define TDS_ATTEN ADC_ATTEN_DB_6 // 1.75V max
#define AIN_COUNT 4
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
static int tds_dev_idx = -1;
static struct {
    bool enabled;
    float voltage_0, voltage_100, voltage_perc;
} moisture_cfg;

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
    memset(&moisture_cfg, 0, sizeof(moisture_cfg));

    adc_atten_t atten = driver_config_get_int(cJSON_GetObjectItem(self->config, OPT_ATTEN), ADC_ATTEN_DB_11);
    samples = driver_config_get_int(cJSON_GetObjectItem(self->config, OPT_SAMPLES), 64);
    update_period = driver_config_get_int(cJSON_GetObjectItem(self->config, OPT_PERIOD), 1000);
    cJSON *moisture_j = cJSON_GetObjectItem(self->config, OPT_MOISTURE);
    moisture_cfg.enabled = driver_config_get_bool(cJSON_GetObjectItem(moisture_j, OPT_ENABLED), false);
    moisture_cfg.voltage_0 = driver_config_get_float(cJSON_GetObjectItem(moisture_j, "voltage_0"), 2.2f);
    moisture_cfg.voltage_100 = driver_config_get_float(cJSON_GetObjectItem(moisture_j, "voltage_100"), 0.9f);
    moisture_cfg.voltage_perc = (moisture_cfg.voltage_0 - moisture_cfg.voltage_100) / 100.0f;
    tds_dev_idx = moisture_cfg.enabled ? AIN_COUNT * 2 : AIN_COUNT;

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
        snprintf(dev.name, sizeof(dev.name), FMT_ADC_SENSOR_NAME, settings.node.name, c);
        cvector_push_back(self->devices, dev);
    }

    if (moisture_cfg.enabled)
        for (size_t c = 0; c < AIN_COUNT; c++)
        {
            memset(&dev, 0, sizeof(dev));
            dev.type = DEV_SENSOR;
            dev.sensor.precision = 1;
            dev.sensor.update_period = update_period;
            strncpy(dev.sensor.measurement_unit, DEV_MU_MOISTURE, sizeof(dev.sensor.measurement_unit));
            strncpy(dev.device_class, DEV_CLASS_MOISTURE, sizeof(dev.device_class));
            snprintf(dev.uid, sizeof(dev.uid), FMT_MOISTURE_SENSOR_ID, c);
            snprintf(dev.name, sizeof(dev.name), FMT_MOISTURE_SENSOR_NAME, settings.node.name, c);
            cvector_push_back(self->devices, dev);
        }

    memset(&dev, 0, sizeof(dev));
    dev.type = DEV_SENSOR;
    dev.sensor.precision = 3;
    dev.sensor.update_period = update_period;
    strncpy(dev.sensor.measurement_unit, DEV_MU_VOLTAGE, sizeof(dev.sensor.measurement_unit));
    strncpy(dev.device_class, DEV_CLASS_VOLTAGE, sizeof(dev.device_class));
    strncpy(dev.uid, FMT_TDS_SENSOR_ID, sizeof(dev.uid));
    snprintf(dev.name, sizeof(dev.name), FMT_TDS_SENSOR_NAME, settings.node.name);
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

    TickType_t period = pdMS_TO_TICKS(update_period);

    while (true)
    {
        TickType_t start = xTaskGetTickCount();

        memset(ain_voltages, 0, sizeof(ain_voltages));
        tds_voltage = 0;

        for (size_t i = 0; i < samples; i++)
        {
            for (size_t c = 0; c < AIN_COUNT; c++)
                ain_voltages[c] += adc_read_voltage(self, ain_channels[c]);

            tds_voltage += adc_read_voltage(self, TDS_CHANNEL);
        }

        for (size_t c = 0; c < AIN_COUNT; c++)
        {
            float voltage = (float)ain_voltages[c] / (float)samples / 1000.0f;
            self->devices[c].sensor.value = voltage;
            driver_send_device_update(self, &self->devices[c]);
        }

        if (moisture_cfg.enabled)
            for (size_t c = 0; c < AIN_COUNT; c++)
            {
                float moisture;
                if (self->devices[c].sensor.value <= moisture_cfg.voltage_100)
                    moisture = 100;
                else if (self->devices[c].sensor.value >= moisture_cfg.voltage_0)
                    moisture = 0;
                else
                    moisture = (moisture_cfg.voltage_0 - self->devices[c].sensor.value) / moisture_cfg.voltage_perc;
                self->devices[c + AIN_COUNT].sensor.value = moisture;
                driver_send_device_update(self, &self->devices[c + AIN_COUNT]);
            }

        self->devices[tds_dev_idx].sensor.value = (float)tds_voltage / (float)samples / 1000.0f;
        driver_send_device_update(self, &self->devices[tds_dev_idx]);

        while (xTaskGetTickCount() - start < period)
        {
            if (!(xEventGroupGetBits(self->eg) & DRIVER_BIT_START))
                return;
            vTaskDelay(1);
        }
    }
}

driver_t drv_gh_adc = {
    .name = "gh_adc",
    .stack_size = DRIVER_GH_ADC_STACK_SIZE,
    .priority = tskIDLE_PRIORITY + 1,
    .defconfig = "{ \"" OPT_PERIOD "\": 2000, \"" OPT_SAMPLES "\": 64, \"" OPT_ATTEN "\": 3, " \
        "\"" OPT_MOISTURE "\": { \"voltage_0\": 2.2, \"voltage_100\": 0.9, \"" OPT_ENABLED "\": true } }",

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

#endif
