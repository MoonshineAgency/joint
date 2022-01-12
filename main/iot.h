#ifndef ESP_IOT_NODE_PLUS_IOT_H
#define ESP_IOT_NODE_PLUS_IOT_H

#include "common.h"
#include "mqtt.h"
#include <stdexcept>
#include <string>
#include <functional>
#include <vector>
#include <map>

namespace iot {

enum mode_t
{
    MODE_LOCAL = 0,
    MODE_NETWORK,
    MODE_BOTH,
};

template <typename T>
class basic_source
{
protected:
    std::string _id, _name;
    QueueHandle_t _local_queue;
    mode_t _mode;

public:
    basic_source(const std::string &owner_name, const std::string &id_, size_t local_queue_len)
        : _id(id_), _mode(MODE_LOCAL)
    {
        _local_queue = xQueueCreate(local_queue_len, sizeof(T));
        if (!_local_queue)
        {
            ESP_LOGE(TAG, "Cannot create queue (len=%d, size=%d)", local_queue_len, sizeof(T));
            return;
        }
        _name = owner_name + "/" + _id;
    }

    virtual ~basic_source()
    {
        if (_local_queue)
            vQueueDelete(_local_queue);
    }

    const std::string &id() { return _id; }

    const std::string &name() { return _name; }

    mode_t mode() { return _mode; }

    void set_mode(mode_t val) { _mode = val; }

    virtual std::string data_to_mqtt(const T &data) = 0;

    esp_err_t send(const T &data)
    {
        if (_mode != MODE_NETWORK && _local_queue && xQueueSend(_local_queue, &data, 0) != pdPASS)
        {
            ESP_LOGW(TAG, "source '%s' local overflow", _name.c_str());
            xQueueOverwrite(_local_queue, &data);
        }
        // FIXME : replace MQTT to more abstract network interface
        if (_mode != MODE_LOCAL && mqtt_is_connected())
        {
            std::string raw = data_to_mqtt(data);
            ESP_LOGI(TAG, "float: %f, Raw: %s", data, raw.c_str());
            if (esp_mqtt_client_enqueue(mqtt_client, _name.c_str(), raw.c_str(), raw.length(), 2, 0, true) < 0)
                ESP_LOGW(TAG, "source '%s' cannot publish to MQTT topic", _name.c_str());
        }
        return ESP_OK;
    }
};

template <typename T>
class numeric_source : public basic_source<T>
{
public:
    numeric_source(const std::string &owner_name, const std::string &id_, size_t local_queue_len)
        : basic_source<T>(owner_name, id_, local_queue_len)
    {}

    virtual std::string data_to_mqtt(const T &data) { return std::to_string(data); }
};

class string_source : public basic_source<std::string>
{
public:
    virtual std::string data_to_mqtt(const std::string &data) { return data; }
};



template <typename T>
class basic_sink
{
protected:
    std::string _id, _name;

public:
    basic_sink(const std::string &owner_name, const std::string &id_)
        : _id(id_)
    {
        _name = owner_name + "/" + _id;
    }

    virtual ~basic_sink() = default;

    const std::string &id() { return _id; }

    const std::string &name() { return _name; }

    virtual std::vector<T> receive(uint32_t timeout_ms) = 0;

    virtual void connect(const std::string &source_id) = 0;

    virtual void disconnect(const std::string &source_id) = 0;
};

template <typename T>
class local_sink : public basic_sink<T>
{
public:
    std::vector<QueueHandle_t> queues;
    //std::map<>

    local_sink(const std::string &owner_name, const std::string &id_)
        : basic_sink<T>(owner_name, id_)
    {}

    virtual std::vector<T> receive(uint32_t timeout_ms)
    {
        std::vector<T> res;
        T data;

        for (auto i = queues.begin(); i != queues.end(); i++)
        {
            if (*i == nullptr)
                continue;
            if (xQueueReceive(*i, &data, 0))
                res.push_back(data);
        }

        return res;
    }
};

template <typename T>
class mqtt_sink : public basic_sink<T>
{
public:
    QueueHandle_t queue;

    mqtt_sink(const std::string &owner_name, const std::string &id_, size_t queue_len)
            : basic_sink<T>(owner_name, id_)
    {
        queue = xQueueCreate(queue_len, sizeof(T));
    }

    virtual ~mqtt_sink()
    {
        if (queue)
            vQueueDelete(queue);
    }

    virtual std::vector<T> receive(uint32_t timeout_ms)
    {

    }

    virtual void connect(const std::string &source_id)
    {

    }

    virtual void disconnect(const std::string &source_id)
    {

    }
};

} /* namespace iot */

#endif //ESP_IOT_NODE_PLUS_IOT_H
