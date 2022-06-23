#include "drv_aht.h"
#include <cstring>
#include <cJSON.h>
#include "mqtt.h"

static const char *ltag = "aht_driver_t";

esp_err_t aht_driver_t::init()
{
    if (!_config["stack_size"])
        _config["stack_size"] = CONFIG_AHT_DRIVER_DEFAULT_STACK_SIZE;

    CHECK(periodic_driver_t::init());

    int dev_count = _config["devices"];
    if (!dev_count)
    {
        ESP_LOGW(ltag, "No devices configured");
        return ESP_ERR_NOT_FOUND;
    }

    for (size_t i = 0; i < dev_count; i++)
    {
        aht_t dev;
        std::memset(&dev, 0, sizeof(dev));

        std::string key = std::string("device") + std::to_string(i) + "_";

        uint8_t addr = _config[key + "address"];
        if (!addr)
            addr = AHT_I2C_ADDRESS_GND;

        auto r = aht_init_desc(&dev, addr, _config[key + "port"],
            static_cast<gpio_num_t>(_config[key + "sda"]),
            static_cast<gpio_num_t>(_config[key + "scl"]));
        if (r != ESP_OK)
        {
            ESP_LOGW(ltag, "Could not initialize device descriptor %d: %d (%s)", i, r, esp_err_to_name(r));
            continue;
        }

        dev.type = static_cast<aht_type_t>(_config[key + "type"]);

        uint32_t freq = _config[key + "frequency"];
        if (freq)
            dev.i2c_dev.cfg.master.clk_speed = freq;

        char name[32] = { 0 };
        std::snprintf(name, sizeof(name), "aht_%d_%d_%d_%02x", i, dev.i2c_dev.cfg.scl_io_num, dev.i2c_dev.cfg.sda_io_num, dev.i2c_dev.addr);

        r = aht_init(&dev);
        if (r != ESP_OK)
        {
            ESP_LOGW(ltag, "Could not initialize device %d ('%s'): %d (%s)", i, name, r, esp_err_to_name(r));
            continue;
        }

        _devices[name] = dev;
        ESP_LOGI(ltag, "Device '%s' started", name);
    }

    return ESP_OK;
}

esp_err_t aht_driver_t::done()
{
    for (auto &i:_devices)
    {
        auto r = aht_free_desc(&i.second);
        if (r != ESP_OK)
            ESP_LOGW(ltag, "Error closing device '%s': %d (%s)", i.first.c_str(), r, esp_err_to_name(r));
        ESP_LOGI(ltag, "Closed device '%s'", i.first.c_str());
    }

    return ESP_OK;
}

void aht_driver_t::loop()
{
    for (auto &i:_devices)
    {
        bool calibrated;
        auto r = aht_get_status(&i.second, nullptr, &calibrated);
        if (r != ESP_OK)
        {
            ESP_LOGW(ltag, "Error reading device '%s' state: %d (%s)", i.first.c_str(), r, esp_err_to_name(r));
            continue;
        }

        float rh, t;
        r = aht_get_data(&i.second, &t, &rh);
        if (r != ESP_OK)
        {
            ESP_LOGW(ltag, "Error reading device '%s' data: %d (%s)", i.first.c_str(), r, esp_err_to_name(r));
            continue;
        }

        // forming cjson
        auto msg = cJSON_CreateObject();
        cJSON_AddBoolToObject(msg, "calibrated", calibrated);
        cJSON_AddNumberToObject(msg, "humidity", rh);
        cJSON_AddNumberToObject(msg, "temperature", t);
        mqtt_client_t::get()->publish_json(i.first, msg, 0, 0);
        cJSON_Delete(msg);
    }
}
