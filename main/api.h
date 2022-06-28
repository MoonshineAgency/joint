#ifndef ESP_IOT_NODE_PLUS_API_H_
#define ESP_IOT_NODE_PLUS_API_H_

#include <esp_err.h>
#include <esp_http_server.h>

/*
 * GET  /api/info
 * GET  /api/reboot
 * GET  /api/settings/reset
 * GET  /api/settings/wifi
 * POST /api/settings/wifi
 * GET  /api/settings/mqtt
 * POST /api/settings/mqtt
 */

esp_err_t api_init(httpd_handle_t server);

#endif // ESP_IOT_NODE_PLUS_API_H_
