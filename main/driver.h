#ifndef ESP_IOT_NODE_PLUS_DRIVER_H_
#define ESP_IOT_NODE_PLUS_DRIVER_H_

#include "common.h"
#include <string>
#include <unordered_map>
#include <utility>

class driver_t
{
public:
    typedef std::unordered_map<std::string, int> config_t;

private:
    static void _task(void *arg)
    {
        if (arg != nullptr)
            ((driver_t *)(arg))->_run();
    }

    virtual void _run() = 0;

protected:
    TaskHandle_t _handle = nullptr;
    config_t _config;

    esp_err_t create_task();

    static void sleep(uint32_t ms)
    {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }

    static uint32_t runtime()
    {
        return pdTICKS_TO_MS(xTaskGetTickCount());
    }

public:
    driver_t(const driver_t&) = delete;
    driver_t &operator=(const driver_t&) = delete;

    explicit driver_t(config_t config)
        : _config(std::move(config))
    {}

    virtual esp_err_t init() = 0;
    virtual esp_err_t start() = 0;
    virtual esp_err_t stop() = 0;
    virtual esp_err_t done() = 0;

    virtual void suspend();
    virtual void resume();
};


#define STATE_BIT_EXECUTING BIT(0)

class periodic_driver_t : public driver_t
{
    using driver_t::driver_t;

protected:
    size_t _loop_no = 0;

private:
    EventGroupHandle_t _state = nullptr;

    void _run() override;
    virtual void loop() = 0;

public:
    esp_err_t init() override;
    esp_err_t start() override;
    esp_err_t stop() override;
    esp_err_t done() override;
};

#endif // ESP_IOT_NODE_PLUS_DRIVER_H_
