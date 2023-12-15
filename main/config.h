#ifndef MAIN_CONFIG_H_
#define MAIN_CONFIG_H_

////////////////////////////////////////////////////////////////////////////////
/// System

#define APP_NAME "joint"
#define SYSTEM_ID_LEN 64

////////////////////////////////////////////////////////////////////////////////
/// Hardware

#if CONFIG_BOARD_GH_3X
#include "boards/gh_3x.h"
#elif CONFIG_BOARD_GH_4DEV
#include "boards/gh_4dev.h"
#else
#error Invalid target board
#endif

////////////////////////////////////////////////////////////////////////////////
/// Date/Time

#define DEFAULT_TZ "UTC"
#define DEFAULT_DT_FMT "%a %d.%m.%Y %H:%M:%S"
#define DEFAULT_SNTP_INTERVAL (60 * 10)

////////////////////////////////////////////////////////////////////////////////
/// Settings

#define SETTINGS_PARTITION "settings"
#define SETTINGS_MAGIC_KEY "magic"
#define SETTINGS_DATA_KEY "data"
#define SETTINGS_MAGIC_VAL 0xC0DE0005

#if CONFIG_NODE_WIFI_DHCP
    #define DEFAULT_WIFI_DHCP true
#else
    #define DEFAULT_WIFI_DHCP false
#endif

#define DEFAULT_WIFI_AP_MAXCONN 5

////////////////////////////////////////////////////////////////////////////////
/// Bus

#define BUS_QUEUE_LEN 5
#define BUS_TIMEOUT_MS 500
#define BUS_EVENT_DATA_SIZE 64

////////////////////////////////////////////////////////////////////////////////
/// Main task

#define MAIN_TASK_STACK_SIZE 8192
#define MAIN_TASK_PRIORITY 5

////////////////////////////////////////////////////////////////////////////////
/// Safe mode task

#define SAFEMODE_TASK_STACK_SIZE 8192
#define SAFEMODE_TASK_PRIORITY 5

////////////////////////////////////////////////////////////////////////////////
/// Node task

#define NODE_TASK_STACK_SIZE 8192
#define NODE_TASK_PRIORITY (tskIDLE_PRIORITY + 1)
#define NODE_QUEUE_SIZE 20

////////////////////////////////////////////////////////////////////////////////
/// Webserver

#define HTTPD_STACK_SIZE 16384
#define MAX_POST_SIZE 4096

////////////////////////////////////////////////////////////////////////////////
/// MQTT

#define MQTT_BUFFER_SIZE 8192
#define MQTT_OUT_BUFFER_SIZE 16384
#define MQTT_TIMEOUT_MS 5000
#define MQTT_MAX_TOPIC_LEN 256

#endif /* MAIN_CONFIG_H_ */
