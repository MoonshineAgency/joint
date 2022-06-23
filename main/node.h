#ifndef ESP_IOT_NODE_PLUS_NODE_H_
#define ESP_IOT_NODE_PLUS_NODE_H_

#include <esp_err.h>

namespace node {

esp_err_t init();

esp_err_t online();

esp_err_t offline();

} // namespace node

#endif // ESP_IOT_NODE_PLUS_NODE_H_
