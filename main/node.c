#include "node.h"
#include "common.h"
#include "settings.h"
#include "system.h"
#include "cvector.h"
#include "driver.h"
#include "mqtt.h"

#include "drivers/drv_aht.h"
#include "drivers/drv_ds18x20.h"
#include "drivers/drv_gh_io.h"
#include "drivers/drv_gh_adc.h"
#include "drivers/drv_ph_meter.h"

#define DRIVER_CONFIG_TOPIC         "drivers/%s/config"
#define DRIVER_STATE_TOPIC          "drivers/%s/state"
#define DRIVER_COMMAND_TOPIC        "drivers/%s/command"
#define DRIVER_SET_CONFIG_TOPIC     "drivers/%s/config/set"

static bool initialized = false;
static driver_t * const drivers[] = {
    &drv_aht, &drv_ds18x20, &drv_gh_io, &drv_gh_adc, &drv_ph_meter
};
static const size_t driver_count = sizeof(drivers) / sizeof(drivers[0]);
static char buf[1024] = { 0 };

static void publish_driver(const driver_t *driver)
{
    char topic[64] = { 0 };
    snprintf(topic, sizeof(topic), DRIVER_CONFIG_TOPIC, driver->name);
    mqtt_publish_json_subtopic(topic, driver->config, 2, 1);

    snprintf(topic, sizeof(topic), DRIVER_STATE_TOPIC, driver->name);
    const char *state = driver_state_to_name(driver->state);
    mqtt_publish_subtopic(topic, state, strlen(state), 2, 1);
}

static void on_set_config(const char *topic, const char *data, size_t data_len, void *ctx)
{
    if (data_len > sizeof(buf) - 1)
    {
        ESP_LOGE(TAG, "Data too big: %d", data_len);
        return;
    }

    char name[32];
    sscanf(topic, DRIVER_SET_CONFIG_TOPIC, name);

    driver_t *drv = NULL;
    for (size_t i = 0; i < driver_count; i++)
        if (!strncmp(drivers[i]->name, name, sizeof(name)))
        {
            drv = drivers[i];
            break;
        }
    if (!drv)
    {
        ESP_LOGE(TAG, "Unknown driver '%s', ignoring attempt to set config", name);
        return;
    }

    memcpy(buf, data, data_len);
    buf[data_len] = 0;
    esp_err_t r = settings_save_driver_config(drv->name, buf);
    if (r != ESP_OK)
    {
        ESP_LOGE(TAG, "Error saving driver config for '%s': %d (%s)", drv->name, r, esp_err_to_name(r));
        return;
    }

    r = driver_stop(drv);
    if (r != ESP_OK)
        ESP_LOGW(TAG, "Error stopping driver '%s', but restarting anyway: %d (%s)", drv->name, r, esp_err_to_name(r));

    r = driver_init(drv, data, data_len);
    if (r != ESP_OK)
        ESP_LOGW(TAG, "Error initializing driver %s: %d (%s)", drv->name, r, esp_err_to_name(r));

    publish_driver(drv);
}

////////////////////////////////////////////////////////////////////////////////

esp_err_t node_init()
{
    ESP_LOGI(TAG, "Initializing node %s...", settings.node.name);
    system_set_mode(MODE_OFFLINE);

    for (size_t i = 0; i < driver_count; i++)
    {
        esp_err_t r = settings_load_driver_config(drivers[i]->name, buf, sizeof(buf));
        if (r != ESP_OK)
        {
            settings_save_driver_config(drivers[i]->name, drivers[i]->defconfig);
            memcpy(buf, drivers[i]->defconfig, strlen(drivers[i]->defconfig) + 1);
        }

        r = driver_init(drivers[i], buf, strlen(buf));
        if (r != ESP_OK)
            ESP_LOGW(TAG, "Error initializing driver %s: %d (%s)", drivers[i]->name, r, esp_err_to_name(r));
    }

    return ESP_OK;
}

esp_err_t node_online()
{
    if (system_mode() == MODE_ONLINE)
        return ESP_OK;

    ESP_LOGI(TAG, "Node goes online...");
    system_set_mode(MODE_ONLINE);

    if (!initialized)
    {
        for (size_t i = 0; i < driver_count; i++)
        {
            esp_err_t r = driver_start(drivers[i]);
            if (r != ESP_OK)
                ESP_LOGW(TAG, "Error starting driver %s: %d (%s)", drivers[i]->name, r, esp_err_to_name(r));
            publish_driver(drivers[i]);
        }
        initialized = true;
    }
    else
    {
        for (size_t i = 0; i < driver_count; i++)
        {
            esp_err_t r = driver_resume(drivers[i]);
            if (r != ESP_OK)
                ESP_LOGW(TAG, "Error resuming driver %s: %d (%s)", drivers[i]->name, r, esp_err_to_name(r));
            publish_driver(drivers[i]);
        }
    }

    for (size_t i = 0; i < driver_count; i++)
    {
        char topic[64] = { 0 };
        snprintf(topic, sizeof(topic), DRIVER_SET_CONFIG_TOPIC, drivers[i]->name);
        mqtt_subscribe_subtopic(topic, on_set_config, 2, NULL);
    }

    return ESP_OK;
}

esp_err_t node_offline()
{
    if (system_mode() == MODE_OFFLINE)
        return ESP_OK;

    ESP_LOGI(TAG, "Node goes offline...");
    system_set_mode(MODE_OFFLINE);

    for (size_t i = 0; i < driver_count; i++)
    {
        esp_err_t r = driver_suspend(drivers[i]);
        if (r != ESP_OK)
            ESP_LOGW(TAG, "Error suspending driver %s: %d (%s)", drivers[i]->name, r, esp_err_to_name(r));
    }

    return ESP_OK;
}
