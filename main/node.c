#include "node.h"
#include "common.h"
#include "settings.h"
#include "system.h"
#include "cvector.h"
#include "driver.h"
#include "mqtt.h"

#ifdef DRIVER_GH_IO
#include "drivers/gh_io.h"
#endif

#ifdef DRIVER_GH_ADC
#include "drivers/gh_adc.h"
#endif

#ifdef DRIVER_GH_PH_METER
#include "drivers/gh_ph_meter.h"
#endif

#ifdef DRIVER_AHTXX
#include "drivers/ahtxx.h"
#endif

#ifdef DRIVER_DS18B20
#include "drivers/ds18b20.h"
#endif

#ifdef DRIVER_DHTXX
#include "drivers/dhtxx.h"
#endif

#define DRIVER_CONFIG_TOPIC_FMT      "drivers/%s/config"
#define DRIVER_SET_CONFIG_TOPIC_FMT  "drivers/%s/set_config"

static char buf[1024];
static cvector_vector_type(driver_t *)drivers = NULL;
static QueueHandle_t node_queue = NULL;

static void publish_driver(const driver_t *drv)
{
    if (!drv->config)
        return;

    char topic[128] = {0};
    snprintf(topic, sizeof(topic), DRIVER_CONFIG_TOPIC_FMT, drv->name);
    mqtt_publish_json_subtopic(topic, drv->config, 2, 1);
}

static void on_driver_start(driver_t *driver)
{
    if (driver->state != DRIVER_RUNNING)
        return;
    for (size_t d = 0; d < cvector_size(driver->devices); d++)
    {
        device_subscribe(&driver->devices[d]);
        vTaskDelay(1);
        device_publish_discovery(&driver->devices[d]);
        vTaskDelay(1);
        device_publish_state(&driver->devices[d]);
        vTaskDelay(1);
    }
}

static void on_set_config(const char *topic, const char *data, size_t data_len, void *ctx)
{
    if (data_len > sizeof(buf) - 1)
    {
        ESP_LOGE(TAG, "Data too big: %d", data_len);
        return;
    }

    driver_t *drv = (driver_t *)ctx;
    if (!drv->defconfig)
    {
        ESP_LOGE(TAG, "Driver '%s' not supporting config", drv->name);
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

    if (system_mode() == MODE_ONLINE)
    {
        r = driver_start(drv);
        if (r != ESP_OK)
            ESP_LOGW(TAG, "Error initializing driver %s: %d (%s)", drv->name, r, esp_err_to_name(r));

        // publish driver config
        publish_driver(drv);

        // resend devices discovery and resub
        on_driver_start(drv);
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
    ESP_LOGI(TAG, "Initializing node %s...", settings.system.name);

    node_queue = xQueueCreate(100, sizeof(driver_event_t));
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

#ifdef DRIVER_GH_IO
    cvector_push_back(drivers, &drv_gh_io);
#endif
#ifdef DRIVER_GH_ADC
    cvector_push_back(drivers, &drv_gh_adc);
#endif
#ifdef DRIVER_GH_PH_METER
    cvector_push_back(drivers, &drv_ph_meter);
#endif
#ifdef DRIVER_AHTXX
    cvector_push_back(drivers, &drv_aht);
#endif
#ifdef DRIVER_DS18B20
    cvector_push_back(drivers, &drv_ds18b20);
#endif
#ifdef DRIVER_DHTXX
    cvector_push_back(drivers, &drv_dht);
#endif

    system_set_mode(MODE_OFFLINE);

    esp_err_t r;
    for (size_t i = 0; i < cvector_size(drivers); i++)
    {
        drivers[i]->event_queue = node_queue;
        if (drivers[i]->defconfig)
        {
            r = settings_load_driver_config(drivers[i]->name, buf, sizeof(buf));
            if (r != ESP_OK)
            {
                settings_save_driver_config(drivers[i]->name, drivers[i]->defconfig);
                memcpy(buf, drivers[i]->defconfig, strlen(drivers[i]->defconfig) + 1);
            }
        }
        else
        {
            buf[0] = 0;
        }

        r = driver_init(drivers[i], buf, strlen(buf));
        if (r != ESP_OK)
            ESP_LOGW(TAG, "Error initializing driver %s: %d (%s)", drivers[i]->name, r, esp_err_to_name(r));
    }

    return ESP_OK;
}

void node_online()
{
    if (system_mode() == MODE_ONLINE)
        return;

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
        char topic[128] = {0};
        snprintf(topic, sizeof(topic), DRIVER_SET_CONFIG_TOPIC_FMT, drivers[i]->name);
        mqtt_subscribe_subtopic(topic, on_set_config, 2, drivers[i]);
        vTaskDelay(1);
    }

    // resend devices discovery and resub
    for (size_t i = 0; i < cvector_size(drivers); i++)
        on_driver_start(drivers[i]);
}

void node_offline()
{
    if (system_mode() == MODE_OFFLINE)
        return;

    ESP_LOGI(TAG, "Node goes offline...");
    system_set_mode(MODE_OFFLINE);
}
