#ifndef _BASIC_SETTINGS_H_
#define _BASIC_SETTINGS_H_

#include <cstring>
#include <esp_log.h>
#include <esp_check.h>
#include <nvs_flash.h>
#include <nvs.h>

#ifndef OPT_MAGIC
#define OPT_MAGIC "magic"
#endif

template< typename T >
class basic_settings_t
{
private:
    const char *_name = nullptr;
    uint32_t _magic;
    const T *_defaults;

protected:
    virtual esp_err_t _on_reset() { return ESP_OK; };

public:
    T data = {};

    basic_settings_t(const char *name, uint32_t magic, const T *defaults)
        : _name(name), _magic(magic), _defaults(defaults)
    {}

    virtual esp_err_t init()
    {
        ESP_LOGI(_name, "Initializing NVS storage...");
        esp_err_t res = nvs_flash_init();
        if (res == ESP_ERR_NVS_NO_FREE_PAGES || res == ESP_ERR_NVS_NEW_VERSION_FOUND)
        {
            ESP_LOGW(_name, "Formatting NVS storage...");
            ESP_RETURN_ON_ERROR(nvs_flash_erase(), _name, "Error erasing flash");
            ESP_RETURN_ON_ERROR(nvs_flash_init(), _name, "Error initializing flash");
            return reset();
        }
        return res;
    }

    virtual esp_err_t reset()
    {
        ESP_LOGW(_name, "Resetting settings to defaults...");
        std::memcpy(&data, _defaults, sizeof(T));
        ESP_RETURN_ON_ERROR(_on_reset(), _name, "Could not reset settings");
        return save();
    }

    virtual esp_err_t load()
    {
        ESP_LOGI(_name, "Reading settings from '%s'...", _name);

        nvs_handle_t nvs;
        size_t tmp;
        uint32_t magic_val = 0;

        esp_err_t res = nvs_open(_name, NVS_READONLY, &nvs);
        if (res != ESP_OK && res != ESP_ERR_NVS_NOT_FOUND)
            ESP_RETURN_ON_ERROR(res, _name, "Could not open NVS");
        if (res == ESP_ERR_NVS_NOT_FOUND)
            goto exit;

        res = nvs_get_u32(nvs, OPT_MAGIC, &magic_val);
        if (res == ESP_OK && magic_val != _magic)
        {
            ESP_LOGW(_name, "Invalid magic 0x%08x, expected 0x%08x", magic_val, _magic);
            res = ESP_FAIL;
        }
        if (res != ESP_OK)
            goto exit;

        tmp = sizeof(T);
        res = nvs_get_blob(nvs, _name, &data, &tmp);
        if (tmp != sizeof(T))
        {
            ESP_LOGW(_name, "Invalid settings size");
            res = ESP_FAIL;
        }
        if (res != ESP_OK)
            ESP_LOGE(_name, "Error reading settings %d (%s)", res, esp_err_to_name(res));

    exit:
        nvs_close(nvs);
        return res == ESP_OK ? res : reset();
    }

    virtual esp_err_t save()
    {
        ESP_LOGI(_name, "Saving settings to '%s'...", _name);

        nvs_handle_t nvs;
        ESP_RETURN_ON_ERROR(
            nvs_open(_name, NVS_READWRITE, &nvs),
            _name, "Could not open NVS to write");
        ESP_RETURN_ON_ERROR(
            nvs_set_u32(nvs, OPT_MAGIC, _magic),
            _name, "Error writing NVS magic");
        ESP_RETURN_ON_ERROR(
            nvs_set_blob(nvs, _name, &data, sizeof(T)),
            _name, "Error writing NVS settings");
        nvs_close(nvs);

        return ESP_OK;
    }
};

#endif // _BASIC_SETTINGS_H_
