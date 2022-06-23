#ifndef ESP_IOT_NODE_PLUS_COMMON_H_
#define ESP_IOT_NODE_PLUS_COMMON_H_

#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>

#include "config.h"

extern const char *TAG;
extern const char *SYSTEM_ID;

#define CHECK(x) \
    do { \
        esp_err_t __; \
        if ((__ = (x)) != ESP_OK) \
            return __; \
    } while (0)

#define CHECK_ARG(VAL) \
    do { \
        if (!(VAL)) \
            return ESP_ERR_INVALID_ARG; \
    } while (0)

#define CHECK_LOGE(x, msg, ...) \
    do { \
        esp_err_t __; \
        if ((__ = (x)) != ESP_OK) { \
            ESP_LOGE(TAG, msg, ## __VA_ARGS__); \
            return __; \
        } \
    } while (0)

#define CHECK_LOGW(x, msg, ...) \
    do { \
        esp_err_t __; \
        if ((__ = (x)) != ESP_OK) { \
            ESP_LOGW(TAG, msg, ## __VA_ARGS__); \
        } \
    } while (0)

#endif // ESP_IOT_NODE_PLUS_COMMON_H_
