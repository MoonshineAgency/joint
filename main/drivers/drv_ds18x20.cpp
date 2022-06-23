#include "drv_ds18x20.h"
#include "mqtt.h"
#include "cJSON.h"

static const char *ltag = "ds18x20_driver_t";

esp_err_t ds18x20_driver_t::init()
{
    CHECK(periodic_driver_t::init());

    _gpio = static_cast<gpio_num_t>(_config["gpio"]);
    // fixme: valid gpio
    if (_gpio < GPIO_NUM_0 || _gpio >= GPIO_NUM_MAX)
    {
        ESP_LOGE(ltag, "Invalid GPIO number %d", _gpio);
        return ESP_ERR_INVALID_ARG;
    }
    CHECK(gpio_reset_pin(_gpio));

    _rescan = _config["rescan_interval"];
    if (!_rescan)
        _rescan = 1;

    return ESP_OK;
}

void ds18x20_driver_t::loop()
{
    esp_err_t r;

    if (!(_loop_no % _rescan) || !_sensors_found)
    {
        r = ds18x20_scan_devices(_gpio, _sensors.data(), _sensors.size(), &_sensors_found);
        if (r != ESP_OK)
        {
            ESP_LOGW(ltag, "Error scanning bus: %d (%s)", r, esp_err_to_name(r));
            return;
        }
        if (!_sensors_found)
        {
            ESP_LOGW(ltag, "No sensors found");
            return;
        }
        if (_sensors_found > _sensors.size())
            _sensors_found = _sensors.size();
    }

    r = ds18x20_measure_and_read_multi(_gpio, _sensors.data(), _sensors_found, _results.data());
    if (r != ESP_OK)
    {
        ESP_LOGW(ltag, "Error measuring: %d (%s)", r, esp_err_to_name(r));
        return;
    }

    for (size_t i = 0; i < _sensors_found; i++)
    {
        char name[32] = { 0 };
        std::snprintf(name, sizeof(name), "ds18x20_%08x%08x", static_cast<uint32_t>(_sensors[i] >> 32), static_cast<uint32_t>(_sensors[i]));

        auto json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "family", (_sensors[i] & 0xff) == DS18B20_FAMILY_ID ? "DS18B20" : "DS18S20");
        cJSON_AddNumberToObject(json, "temperature", _results[i]);
        mqtt_client_t::get()->publish_json(name, json, 0, 0);
        cJSON_Delete(json);
    }
}
