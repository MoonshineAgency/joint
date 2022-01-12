#include "node.h"
#include "system.h"

#include "test.h"
/**
 * Потоки, устройства и узлы
 *
 *
 * Узел является владельцем устройств
 * ID узла - SYSTEM_ID по умолчанию, settings.node.id
 * Узел читает конфигурацию и создает описанные в ней устройства.
 * Устройства имеют дочерние устройства, параметры и вх/исх потоки.
 *
 * полное имя потока: узел/устройство/поток
 * К одному входящему гнезду можно подключить один исходящий поток
 *
 */

esp_err_t node_init()
{
    ESP_LOGI(TAG, "Initializing node %s...", SYSTEM_ID);

    system_set_mode(MODE_OFFLINE);
    return ESP_OK;
}

esp_err_t node_online()
{
    if (system_mode() == MODE_ONLINE)
        return ESP_OK;

    ESP_LOGI(TAG, "Node goes online...");

    test_run();

    system_set_mode(MODE_ONLINE);
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
