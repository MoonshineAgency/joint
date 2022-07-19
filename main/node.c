#include "node.h"
#include "common.h"
#include "settings.h"
#include "system.h"
#include "cvector.h"
#include "driver.h"
#include "mqtt.h"

#include "drivers/ahtxx.h"
#include "drivers/ds18b20.h"
#include "drivers/gh_io.h"
#include "drivers/gh_adc.h"
#include "drivers/gh_ph_meter.h"

#define DRIVER_CONFIG_TOPIC_FMT      "drivers/%s/config"
#define DRIVER_SET_CONFIG_TOPIC_FMT  "drivers/%s/set_config"

static char buf[1024];
static cvector_vector_type(driver_t *) drivers = NULL;
static QueueHandle_t node_queue = NULL;

static void publish_driver(const driver_t *drv)
{
    char topic[128] = { 0 };
    snprintf(topic, sizeof(topic), DRIVER_CONFIG_TOPIC_FMT, drv->name);
    mqtt_publish_json_subtopic(topic, drv->config, 2, 1);
}

static void on_set_config(const char *topic, const char *data, size_t data_len, void *ctx)
{
    if (data_len > sizeof(buf) - 1)
    {
        ESP_LOGE(TAG, "Data too big: %d", data_len);
        return;
    }

    driver_t *drv = (driver_t *)ctx;

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

    if (system_mode() == MODE_ONLINE)
    {
        r = driver_start(drv);
        if (r != ESP_OK)
            ESP_LOGW(TAG, "Error initializing driver %s: %d (%s)", drv->name, r, esp_err_to_name(r));

        publish_driver(drv);
    }
}

static void node_task(void *arg)
{
    driver_event_t e;
    while (true)
    {
        if (!xQueueReceive(node_queue, &e, portMAX_DELAY))
            continue;
        if (system_mode() != MODE_ONLINE)
            continue;
        switch (e.type)
        {
            case DRV_EVENT_DEVICE_UPDATED:
                device_publish_state(&e.dev);
                break;
            case DRV_EVENT_DEVICE_ADDED:
                device_publish_discovery(&e.dev);
                break;
            case DRV_EVENT_DEVICE_REMOVED:
                device_unsubscribe(&e.dev);
                device_unpublish_discovery(&e.dev);
                break;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

esp_err_t node_init()
{
    ESP_LOGI(TAG, "Initializing node %s...", settings.node.name);

    node_queue = xQueueCreate(40, sizeof(driver_event_t));
    if (!node_queue)
    {
        ESP_LOGE(TAG, "Error creating node queue");
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreatePinnedToCore(node_task, "node_task", NODE_TASK_STACK_SIZE, NULL, NODE_TASK_PRIORITY, NULL, APP_CPU_NUM) != pdPASS)
    {
        ESP_LOGE(TAG, "Error creating node task");
        return ESP_ERR_NO_MEM;
    }

    cvector_push_back(drivers, &drv_gh_io);
    cvector_push_back(drivers, &drv_gh_adc);
    cvector_push_back(drivers, &drv_ph_meter);
    cvector_push_back(drivers, &drv_aht);
    cvector_push_back(drivers, &drv_ds18b20);

    system_set_mode(MODE_OFFLINE);

    for (size_t i = 0; i < cvector_size(drivers); i++)
    {
        drivers[i]->event_queue = node_queue;
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

    for (size_t i = 0; i < cvector_size(drivers); i++)
    {
        if (drivers[i]->state != DRIVER_INITIALIZED)
            continue;
        esp_err_t r = driver_start(drivers[i]);
        if (r != ESP_OK)
            ESP_LOGW(TAG, "Error starting driver %s: %d (%s)", drivers[i]->name, r, esp_err_to_name(r));

        publish_driver(drivers[i]);

        // subscribe on driver config
        char topic[128] = { 0 };
        snprintf(topic, sizeof(topic), DRIVER_SET_CONFIG_TOPIC_FMT, drivers[i]->name);
        mqtt_subscribe_subtopic(topic, on_set_config, 2, drivers[i]);
    }

    // resend devices discovery and resub
    for (size_t i = 0; i < cvector_size(drivers); i++)
    {
        if (drivers[i]->state != DRIVER_RUNNING)
            continue;
        for (size_t d = 0; d < cvector_size(drivers[i]->devices); d++)
        {
            device_subscribe(&drivers[i]->devices[d]);
            device_publish_discovery(&drivers[i]->devices[d]);
        }
    }

    return ESP_OK;
}

esp_err_t node_offline()
{
    if (system_mode() == MODE_OFFLINE)
        return ESP_OK;

    ESP_LOGI(TAG, "Node goes offline...");
    system_set_mode(MODE_OFFLINE);

    return ESP_OK;
}
