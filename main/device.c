#include "device.h"
#include <esp_ota_ops.h>
#include "settings.h"
#include "mqtt.h"
#include "cJSON.h"
#include "common.h"

#define STATE_TOPIC_FMT     "%s/%s/state"
#define DISCOVERY_TOPIC_FMT "homeassistant/%s/%s/%s/config"
#define COMMAND_TOPIC_FMT   "%s/%s/command"

#define QOS_SENSOR_STATE 0
#define RETAIN_SENSOR_STATE 0

#define QOS_EFFECTOR_STATE 2
#define RETAIN_EFFECTOR_STATE 1

#define QOS_DISCOVERY 2
#define RETAIN_DISCOVERY 1

#define EXPIRES_AFTER_PERIODS 5

static const char * const dev_type_names [] = {
    [DEV_SENSOR]        = "sensor",
    [DEV_BINARY_SENSOR] = "binary_sensor",
    [DEV_NUMBER]        = "number",
    [DEV_BINARY_SWITCH] = "switch",
};

static const char *device_state_topic(const device_t *dev, char *buf, size_t size)
{
    snprintf(buf, size, STATE_TOPIC_FMT, settings.node.name, dev->uid);
    return buf;
}

static const char *device_command_topic(const device_t *dev, char *buf, size_t size)
{
    snprintf(buf, size, COMMAND_TOPIC_FMT, settings.node.name, dev->uid);
    return buf;
}

static const char *device_discovery_topic(const device_t *dev, char *buf, size_t size)
{
    const char *type_name = dev_type_names[dev->type];
    if (!type_name)
        type_name = dev->type_name;
    if (!strlen(type_name))
        return NULL;

    snprintf(buf, size, DISCOVERY_TOPIC_FMT, type_name, settings.node.name, dev->uid);
    return buf;
}

static cJSON *device_descriptor(const device_t *dev)
{
    cJSON *res = cJSON_CreateObject();

    if (strlen(dev->device_class))
        cJSON_AddStringToObject(res, "device_class", dev->device_class);

    char uid[sizeof(dev->uid) + sizeof(settings.node.name) + 1] = { 0 };
    snprintf(uid, sizeof(uid), "%s_%s", settings.node.name, dev->uid);
    cJSON_AddStringToObject(res, "object_id", uid);
    cJSON_AddStringToObject(res, "unique_id", uid);

    cJSON_AddStringToObject(res, "name", strlen(dev->name) ? dev->name : uid);

    char buf[128] = { 0 };
    cJSON_AddStringToObject(res, "state_topic", device_state_topic(dev, buf, sizeof(buf)));

    switch (dev->type)
    {
        case DEV_SENSOR:
            cJSON_AddStringToObject(res, "unit_of_measurement", dev->sensor.measurement_unit);
            if (dev->sensor.update_period > 0)
                cJSON_AddNumberToObject(res, "expire_after", dev->sensor.update_period * EXPIRES_AFTER_PERIODS / 1000);
            break;
        case DEV_BINARY_SENSOR:
            cJSON_AddStringToObject(res, "payload_on", "1");
            cJSON_AddStringToObject(res, "payload_off", "0");
            break;
        case DEV_BINARY_SWITCH:
            cJSON_AddStringToObject(res, "command_topic", device_command_topic(dev, buf, sizeof(buf)));
            cJSON_AddStringToObject(res, "payload_on", "1");
            cJSON_AddStringToObject(res, "payload_off", "0");
            cJSON_AddStringToObject(res, "state_on", "1");
            cJSON_AddStringToObject(res, "state_off", "0");
            cJSON_AddBoolToObject(res, "optimistic", false);
            cJSON_AddNumberToObject(res, "qos", 2);
            cJSON_AddBoolToObject(res, "retain", true);
            break;
        case DEV_NUMBER:
            cJSON_AddStringToObject(res, "command_topic", device_command_topic(dev, buf, sizeof(buf)));
            cJSON_AddNumberToObject(res, "min", dev->number.min);
            cJSON_AddNumberToObject(res, "max", dev->number.max);
            cJSON_AddNumberToObject(res, "step", dev->number.step);
            cJSON_AddStringToObject(res, "unit_of_measurement", dev->number.measurement_unit);
            cJSON_AddBoolToObject(res, "optimistic", false);
            cJSON_AddNumberToObject(res, "qos", 2);
            cJSON_AddBoolToObject(res, "retain", true);
            break;
    }

    cJSON *device = cJSON_AddObjectToObject(res, "device");

    const char *ids[2] = {
        settings.node.name,
        SYSTEM_ID,
    };
    cJSON_AddItemToObject(device, "identifiers",
        cJSON_CreateStringArray(ids, strncmp(settings.node.name, SYSTEM_ID, sizeof(settings.node.name)) == 0 ? 1 : 2));

    cJSON_AddStringToObject(device, "manufacturer", DEVICE_MANUFACTURER);
    cJSON_AddStringToObject(device, "name", DEVICE_NAME);
    cJSON_AddStringToObject(device, "model", DEVICE_MODEL);

    const esp_app_desc_t *app_desc = esp_app_get_description();
    snprintf(buf, sizeof(buf), "%s (%s)", app_desc->version, app_desc->date);
    cJSON_AddStringToObject(device, "sw_version", buf);

    return res;
}

//static cJSON *trigger_descriptor(const device_t *dev)
//{
//    if (dev
//}

static void on_write_cb(const char *topic, const char *data, size_t data_len, void *ctx)
{
    device_t *dev = (device_t *)ctx;
    switch (dev->type)
    {
        case DEV_BINARY_SWITCH:
            if (dev->binary_switch.on_write)
                dev->binary_switch.on_write(dev, data[0] == '1');
            break;
        case DEV_NUMBER:
            if (dev->number.on_write)
                dev->number.on_write(dev, strtof(data, NULL));
            break;
        default:
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////

void device_publish_state(device_t *dev)
{
    char data[32] = { 0 };
    int qos = 0;
    int retain = 0;
    switch (dev->type)
    {
        case DEV_SENSOR:
            qos = QOS_SENSOR_STATE;
            retain = RETAIN_SENSOR_STATE;
            snprintf(data, sizeof(data), "%.*f", dev->sensor.precision, dev->sensor.value);
            break;
        case DEV_BINARY_SENSOR:
            qos = QOS_SENSOR_STATE;
            retain = RETAIN_SENSOR_STATE;
            snprintf(data, sizeof(data), "%d", dev->binary_sensor.value);
            break;
        case DEV_NUMBER:
            qos = QOS_EFFECTOR_STATE;
            retain = RETAIN_EFFECTOR_STATE;
            snprintf(data, sizeof(data), "%f", dev->number.value);
            break;
        case DEV_BINARY_SWITCH:
            qos = QOS_EFFECTOR_STATE;
            retain = RETAIN_EFFECTOR_STATE;
            snprintf(data, sizeof(data), "%d", dev->binary_switch.value);
            break;
    }

    char topic[128] = { 0 };
    mqtt_publish(device_state_topic(dev, topic, sizeof(topic)), data, strlen(data), qos, retain);
}

void device_publish_discovery(device_t *dev)
{
    char topic[128] = { 0 };
    if (!device_discovery_topic(dev, topic, sizeof(topic)))
        return;

    cJSON *json = device_descriptor(dev);
    mqtt_publish_json(topic, json, QOS_DISCOVERY, RETAIN_DISCOVERY);
    cJSON_Delete(json);
}

void device_unpublish_discovery(device_t *dev)
{
    char topic[128] = { 0 };
    if (!device_discovery_topic(dev, topic, sizeof(topic)))
        return;
    mqtt_publish(topic, "", 0, QOS_DISCOVERY, RETAIN_DISCOVERY);
}

void device_subscribe(device_t *dev)
{
    char topic[128] = { 0 };
    switch (dev->type)
    {
        case DEV_SENSOR:
        case DEV_BINARY_SENSOR:
            return;
        case DEV_BINARY_SWITCH:
        case DEV_NUMBER:
            mqtt_subscribe(device_command_topic(dev, topic, sizeof(topic)), on_write_cb, 2, dev);
            break;
    }
}

void device_unsubscribe(device_t *dev)
{
    char topic[128] = { 0 };
    mqtt_unsubscribe(device_command_topic(dev, topic, sizeof(topic)), on_write_cb, dev);
}
