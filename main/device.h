#ifndef ESP_IOT_NODE_PLUS_DEVICE_H_
#define ESP_IOT_NODE_PLUS_DEVICE_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    DEV_SENSOR = 0,
    DEV_BINARY_SENSOR,
    DEV_BINARY_SWITCH,
    DEV_NUMBER,
} device_type_t;

typedef struct device device_t;

typedef void (*switch_write_cb_t)(device_t *dev, bool value);
typedef void (*output_write_cb_t)(device_t *dev, float value);

struct device
{
    char uid[32];
    char name[96];
    device_type_t type;
    char type_name[16];
    char device_class[32];
    void *internal[8];
    union {
        struct {
            char measurement_unit[16];
            float value;
            int precision;
            int update_period;
        } sensor;
        struct {
            bool value;
        } binary_sensor;
        struct {
            char measurement_unit[16];
            float min;
            float max;
            float step;
            float value;
            output_write_cb_t on_write;
        } number;
        struct {
            bool value;
            switch_write_cb_t on_write;
        } binary_switch;
    };
};

void device_publish_state(device_t *dev);
void device_publish_discovery(device_t *dev);
void device_unpublish_discovery(device_t *dev);

void device_subscribe(device_t *dev);
void device_unsubscribe(device_t *dev);

#endif // ESP_IOT_NODE_PLUS_DEVICE_H_
