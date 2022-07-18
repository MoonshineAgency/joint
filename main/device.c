#include "device.h"
#include "settings.h"
#include "mqtt.h"
#include "cJSON.h"

#define STATE_TOPIC_FMT     "%s/%s/state"
#define DISCOVERY_TOPIC_FMT "homeassistant/%s/%s/%s/config"
#define COMMAND_TOPIC_FMT   "%s/%s/command"

#define STATE_QOS 0
#define STATE_RETAIN 0

#define DISCOVERY_QOS 2
#define DISCOVERY_RETAIN 1

static const char * const dev_type_names [] = {
    [DEV_SENSOR]        = "sensor",
    [DEV_BINARY_SENSOR] = "binary_sensor",
    [DEV_OUTPUT]        = NULL,
    [DEV_SWITCH]        = "switch",
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

    char topic[128] = { 0 };
    cJSON_AddStringToObject(res, "state_topic", device_state_topic(dev, topic, sizeof(topic)));

    switch (dev->type)
    {
        case DEV_SENSOR:
            cJSON_AddStringToObject(res, "unit_of_measurement", dev->sensor.measurement_unit);
            break;
        case DEV_BINARY_SENSOR:
            cJSON_AddStringToObject(res, "payload_on", "1");
            cJSON_AddStringToObject(res, "payload_off", "0");
            break;
        case DEV_SWITCH:
            cJSON_AddStringToObject(res, "command_topic", device_command_topic(dev, topic, sizeof(topic)));
            cJSON_AddStringToObject(res, "payload_on", "1");
            cJSON_AddStringToObject(res, "payload_off", "0");
            cJSON_AddStringToObject(res, "state_on", "1");
            cJSON_AddStringToObject(res, "state_off", "0");
            cJSON_AddBoolToObject(res, "optimistic", false);
            cJSON_AddNumberToObject(res, "qos", 2);
            cJSON_AddBoolToObject(res, "retain", true);
            break;
        default:
            break;
    }

    return res;
}

static void on_write_cb(const char *topic, const char *data, size_t data_len, void *ctx)
{
    device_t *dev = (device_t *)ctx;
    switch (dev->type)
    {
        case DEV_SWITCH:
            if (dev->binary_switch.on_write)
                dev->binary_switch.on_write(dev, data[0] == '1');
            break;
        case DEV_OUTPUT:
            if (dev->binary_switch.on_write)
                dev->binary_switch.on_write(dev, strtol(data, NULL, 0));
            break;
        default:
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////

void device_publish_state(device_t *dev)
{
    char data[32] = { 0 };
    switch (dev->type)
    {
        case DEV_SENSOR:
            snprintf(data, sizeof(data), "%.*f", dev->sensor.precision, dev->sensor.value);
            break;
        case DEV_BINARY_SENSOR:
            snprintf(data, sizeof(data), "%d", dev->binary_sensor.value);
            break;
        case DEV_OUTPUT:
            snprintf(data, sizeof(data), "%d", dev->output.value);
            break;
        case DEV_SWITCH:
            snprintf(data, sizeof(data), "%d", dev->binary_switch.value);
            break;
    }

    char topic[128] = { 0 };
    mqtt_publish(device_state_topic(dev, topic, sizeof(topic)), data, strlen(data), STATE_QOS, STATE_RETAIN);
}

void device_publish_discovery(device_t *dev)
{
    char topic[128] = { 0 };
    if (!device_discovery_topic(dev, topic, sizeof(topic)))
        return;

    cJSON *json = device_descriptor(dev);
    mqtt_publish_json(topic, json, DISCOVERY_QOS, DISCOVERY_RETAIN);
    cJSON_Delete(json);
}

void device_unpublish_discovery(device_t *dev)
{
    char topic[128] = { 0 };
    if (!device_discovery_topic(dev, topic, sizeof(topic)))
        return;
    mqtt_publish(topic, "", 0, DISCOVERY_QOS, DISCOVERY_RETAIN);
}

void device_subscribe(device_t *dev)
{
    char topic[128] = { 0 };
    switch (dev->type)
    {
        case DEV_SENSOR:
        case DEV_BINARY_SENSOR:
            return;
        case DEV_SWITCH:
        case DEV_OUTPUT:
            mqtt_subscribe(device_command_topic(dev, topic, sizeof(topic)), on_write_cb, 2, dev);
            break;
    }
}

void device_unsubscribe(device_t *dev)
{
    char topic[128] = { 0 };
    mqtt_unsubscribe(device_command_topic(dev, topic, sizeof(topic)), on_write_cb, dev);
}
