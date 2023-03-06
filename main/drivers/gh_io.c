#include "gh_io.h"
#include <esp_log.h>
#include <esp_check.h>
#include "settings.h"
#include <tca95x5.h>
#include "common.h"

#define FMT_RELAY_ID    "relay%d"
#define FMT_INPUT_ID    "input%d"
#define FMT_SWITCH_ID   "switch%d"

#define FMT_RELAY_NAME  "%s relay %d"
#define FMT_INPUT_NAME  "%s isolated input %d"
#define FMT_SWITCH_NAME "%s input switch %d"

#define RELAYS_COUNT 7
#define SWITCHES_COUNT 4
#define INPUTS_COUNT 4
#define PORT_MODE 0xff00 // low 8 bits = input, high 8 bits = output

#define CHANGED_BITS BIT(0)

static i2c_dev_t expander = { 0 };
static EventGroupHandle_t port_event = NULL;
static gpio_num_t intr;
static device_t *switches;
static device_t *inputs;

static void IRAM_ATTR on_port_change(void *arg)
{
    (void)arg;
    BaseType_t hp_task;
    if (xEventGroupSetBitsFromISR(port_event, CHANGED_BITS, &hp_task) != pdFAIL)
        portYIELD_FROM_ISR(hp_task);
}

static void on_relay_command(device_t *dev, bool value)
{
    esp_err_t r = tca95x5_set_level(&expander, (uint32_t)(dev->internal[0]), value);
    if (r != ESP_OK)
    {
        ESP_LOGE(drv_gh_io.name, "Cannot set port value: %d (%s)", r, esp_err_to_name(r));
        return;
    }

    dev->binary_switch.value = value;
    driver_send_device_update(&drv_gh_io, dev);
}

static esp_err_t on_init(driver_t *self)
{
    cvector_free(self->devices);

    if (port_event)
    {
        vEventGroupDelete(port_event);
        port_event = NULL;
    }
    port_event = xEventGroupCreate();
    if (!port_event)
    {
        ESP_LOGE(self->name, "Error creating port_event group");
        return ESP_ERR_NO_MEM;
    }

    gpio_num_t sda = driver_config_get_gpio(cJSON_GetObjectItem(self->config, OPT_SDA), CONFIG_I2C0_SDA_GPIO);
    gpio_num_t scl = driver_config_get_gpio(cJSON_GetObjectItem(self->config, OPT_SCL), CONFIG_I2C0_SCL_GPIO);
    intr = driver_config_get_gpio(cJSON_GetObjectItem(self->config, OPT_INTR), CONFIG_GH_IO_DRIVER_INTR_GPIO);
    uint8_t addr = driver_config_get_int(cJSON_GetObjectItem(self->config, OPT_ADDRESS), TCA95X5_I2C_ADDR_BASE);
    i2c_port_t port = driver_config_get_int(cJSON_GetObjectItem(self->config, OPT_PORT), 0);
    int freq = driver_config_get_int(cJSON_GetObjectItem(self->config, OPT_FREQ), 0);

    memset(&expander, 0, sizeof(expander));
    ESP_RETURN_ON_ERROR(
        tca95x5_init_desc(&expander, addr, port, sda, scl),
        self->name, "Error initializing device descriptor: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );
    if (freq)
        expander.cfg.master.clk_speed = freq;
    ESP_RETURN_ON_ERROR(
        tca95x5_port_write(&expander, 0),
        self->name, "Error writing to port: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );
    ESP_RETURN_ON_ERROR(
        tca95x5_port_set_mode(&expander, PORT_MODE),
        self->name, "Error setting up port mode: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );

    ESP_LOGI(self->name, "Initialized TCA9555: ADDR=0x%02x, PORT=%d, SDA=%d, SCL=%d, FREQ=%d", addr, port, sda, scl, freq);

    ESP_RETURN_ON_ERROR(
        gpio_reset_pin(intr),
        self->name, "Error reset INTR GPIO %d: %d (%s)", intr, err_rc_, esp_err_to_name(err_rc_)
    );
    gpio_set_direction(intr, GPIO_MODE_INPUT);
    gpio_set_intr_type(intr, GPIO_INTR_NEGEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(intr, on_port_change, NULL);

    device_t dev;

    for (int i = 0; i < RELAYS_COUNT; i++)
    {
        memset(&dev, 0, sizeof(dev));
        dev.type = DEV_BINARY_SWITCH;
        dev.internal[0] = (void *)i;
        dev.binary_switch.on_write = on_relay_command;
        snprintf(dev.uid, sizeof(dev.uid), FMT_RELAY_ID, i);
        snprintf(dev.name, sizeof(dev.name), FMT_RELAY_NAME, settings.node.name, i);
        cvector_push_back(self->devices, dev);
    }

    inputs = self->devices + RELAYS_COUNT;
    for (int i = 0; i < INPUTS_COUNT; i++)
    {
        memset(&dev, 0, sizeof(dev));
        dev.type = DEV_BINARY_SENSOR;
        snprintf(dev.uid, sizeof(dev.uid), FMT_INPUT_ID, i);
        snprintf(dev.name, sizeof(dev.name), FMT_INPUT_NAME, settings.node.name, i);
        cvector_push_back(self->devices, dev);
    }

    switches = inputs + INPUTS_COUNT;
    for (int i = 0; i < SWITCHES_COUNT; i++)
    {
        memset(&dev, 0, sizeof(dev));
        dev.type = DEV_BINARY_SENSOR;
        snprintf(dev.uid, sizeof(dev.uid), FMT_SWITCH_ID, i);
        snprintf(dev.name, sizeof(dev.name), FMT_SWITCH_NAME, settings.node.name, i);
        cvector_push_back(self->devices, dev);
    }

    return ESP_OK;
}

static esp_err_t on_start(driver_t *self)
{
    for (size_t i = 0; i < cvector_size(self->devices); i++)
        driver_send_device_update(self, &self->devices[i]);

    return ESP_OK;
}

static void task(driver_t *self)
{
    while (true)
    {
        while (!(xEventGroupGetBits(port_event) & CHANGED_BITS))
        {
            if (!(xEventGroupGetBits(self->eg) & DRIVER_BIT_START))
                return;
            vTaskDelay(1);
        }

        xEventGroupClearBits(port_event, CHANGED_BITS);

        uint16_t val = 0;
        esp_err_t r = tca95x5_port_read(&expander, &val);
        if (r != ESP_OK)
        {
            ESP_LOGE(self->name, "Cannot read port value: %d (%s)", r, esp_err_to_name(r));
            continue;
        }

        uint8_t bits = (val >> 8) & 0x0f;
        for (int i = 0; i < 4; i++)
        {
            bool bit = (bits >> i) & 1;
            if (inputs[i].binary_sensor.value != bit)
            {
                inputs[i].binary_sensor.value = bit;
                driver_send_device_update(self, &inputs[i]);
            }
        }

        bits = (~val >> 12) & 0x0f;
        for (int i = 0; i < 4; i++)
        {
            bool bit = (bits >> i) & 1;
            if (switches[i].binary_sensor.value != bit)
            {
                switches[i].binary_sensor.value = bit;
                driver_send_device_update(self, &switches[i]);
            }
        }
    }
}

static esp_err_t on_stop(driver_t *self)
{
    gpio_isr_handler_remove(intr);
    vEventGroupDelete(port_event);
    port_event = NULL;
    esp_err_t r = tca95x5_free_desc(&expander);
    if (r != ESP_OK)
        ESP_LOGW(self->name, "Device descriptor free error: %d (%s)", r, esp_err_to_name(r));

    return ESP_OK;
}

driver_t drv_gh_io = {
    .name = "gh_io",
    .defconfig = "{ \"" OPT_STACK_SIZE "\": 4096, \"" OPT_PORT "\": 0, \"" OPT_SDA "\": " STR(CONFIG_I2C0_SDA_GPIO) ", \"" OPT_SCL "\": " STR(CONFIG_I2C0_SCL_GPIO)
        ", \"" OPT_INTR "\": " STR(CONFIG_GH_IO_DRIVER_INTR_GPIO) ", \"" OPT_ADDRESS "\": 32 }",

    .config = NULL,
    .state = DRIVER_NEW,
    .event_queue = NULL,

    .devices = NULL,
    .handle = NULL,
    .eg = NULL,

    .on_init = on_init,
    .on_start = on_start,
    .on_stop = on_stop,

    .task = task
};
