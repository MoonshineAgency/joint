#include "drv_gh_io.h"
#include "periodic_driver.h"
#include "common.h"
#include "mqtt.h"
#include <tca95x5.h>

#define RELAYS_COUNT 7
#define PORT_MODE 0xff00 // low 8 bits = input, high 8 bits = output
#define CHANGED_BITS BIT(0)

static i2c_dev_t dev = { 0 };
static EventGroupHandle_t event;
static uint8_t old_switches;
static uint8_t old_inputs;
static uint8_t relays_state = 0;

void IRAM_ATTR on_port_change(void *arg)
{
    BaseType_t hp_task;
    BaseType_t r = xEventGroupSetBitsFromISR(event, CHANGED_BITS, &hp_task);
    if (r != pdFAIL)
        portYIELD_FROM_ISR(hp_task);
}

static void publish_relays_state()
{
    char topic[32] = { 0 };
    for (size_t i = 0; i < RELAYS_COUNT; i++)
    {
        snprintf(topic, sizeof(topic), CONFIG_GH_IO_DRIVER_RELAYS_STATE_TOPIC, i);
        mqtt_publish_subtopic(topic, (relays_state >> i) & 1 ? "1" : "0", 1, 2, 1);
    }
}

void on_relays_command(const char *topic, const char *data, size_t data_len, void *ctx)
{
    size_t relay = (size_t)ctx;

    bool state = data[0] == '1' || !strncasecmp(data, "on", 2) || !strncasecmp(data, "yes", 3);
    relays_state |= state << relay;

    esp_err_t r = tca95x5_set_level(&dev, relay, state);
    if (r != ESP_OK)
    {
        ESP_LOGE(drv_gh_io.name, "Error writing to port: %d, %s", r, esp_err_to_name(r));
        return;
    }

    ESP_LOGI(drv_gh_io.name, "Relay %d switched to %d", relay, state);

    publish_relays_state();
}

static void publish_inputs(const char *topic, uint8_t val, uint8_t old_val)
{
    char full_topic[32] = { 0 };
    for (int i = 0; i < 4; i++)
    {
        snprintf(full_topic, sizeof(full_topic), "%s/%d/state", topic, i);
        mqtt_publish_subtopic(full_topic, (val >> i) & 1 ? "1" : "0", 1, 2, 1);
    }
}

static void publish_inputs_state(driver_t *self)
{
    uint16_t val;
    esp_err_t r = tca95x5_port_read(&dev, &val);
    if (r != ESP_OK)
    {
        ESP_LOGE(self->name, "Cannot read port value: %d (%s)", r, esp_err_to_name(r));
        return;
    }

    uint8_t inputs = (val >> 12) & 0x0f;
    publish_inputs(CONFIG_GH_IO_DRIVER_INPUTS_TOPIC, inputs, old_inputs);
    old_inputs = inputs;

    uint8_t switches = (val >> 8) & 0x0f;
    publish_inputs(CONFIG_GH_IO_DRIVER_SWITCHES_TOPIC, ~switches, old_switches);
    old_switches = switches;
}

static void loop(driver_t *self)
{
    xEventGroupWaitBits(event, CHANGED_BITS, pdTRUE, pdTRUE, portMAX_DELAY);
    publish_inputs_state(self);
}

static esp_err_t init(driver_t *self)
{
    self->internal[0] = loop;

    event = xEventGroupCreate();
    if (!event)
    {
        ESP_LOGE(self->name, "Error creating event group");
        return ESP_ERR_NO_MEM;
    }

    gpio_num_t sda = config_get_gpio(cJSON_GetObjectItem(self->config, "sda"), CONFIG_I2C0_SDA_GPIO);
    gpio_num_t scl = config_get_gpio(cJSON_GetObjectItem(self->config, "scl"), CONFIG_I2C0_SCL_GPIO);
    gpio_num_t intr = config_get_gpio(cJSON_GetObjectItem(self->config, "intr"), CONFIG_GH_IO_DRIVER_INTR_GPIO);
    uint8_t addr = config_get_int(cJSON_GetObjectItem(self->config, "address"), TCA95X5_I2C_ADDR_BASE);
    i2c_port_t port = config_get_int(cJSON_GetObjectItem(self->config, "port"), 0);
    int freq = config_get_int(cJSON_GetObjectItem(self->config, "frequency"), 0);

    ESP_RETURN_ON_ERROR(
        tca95x5_init_desc(&dev, addr, port, sda, scl),
        self->name, "Error initializing device descriptor: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );
    if (freq)
        dev.cfg.master.clk_speed = freq;
    ESP_RETURN_ON_ERROR(
        tca95x5_port_set_mode(&dev, PORT_MODE),
        self->name, "Error setting up port mode: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );
    ESP_RETURN_ON_ERROR(
        tca95x5_port_write(&dev, 0),
        self->name, "Error writing to port: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
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

    for (size_t i = 0; i < RELAYS_COUNT; i++)
    {
        char topic[32] = { 0 };
        snprintf(topic, sizeof(topic), CONFIG_GH_IO_DRIVER_RELAYS_CMD_TOPIC, i);
        mqtt_subscribe_subtopic(topic, on_relays_command, 2, (void *)i);
    }

    old_switches = old_inputs = 0;

    return periodic_driver_init(self);
}

static esp_err_t start(driver_t *self)
{
    publish_inputs_state(self);
    publish_relays_state();

    return periodic_driver_start(self);
}

static esp_err_t resume(driver_t *self)
{
    publish_inputs_state(self);
    publish_relays_state();

    return periodic_driver_resume(self);
}

static esp_err_t stop(driver_t *self)
{
    for (size_t i = 0; i < RELAYS_COUNT; i++)
    {
        char topic[32] = { 0 };
        snprintf(topic, sizeof(topic), CONFIG_GH_IO_DRIVER_RELAYS_CMD_TOPIC, i);
        mqtt_unsubscribe_subtopic(topic, on_relays_command, (void *)i);
    }

    return periodic_driver_stop(self);
}

driver_t drv_gh_io = {
    .name = "drv_gh_io",

    .config = NULL,
    .state = DRIVER_NEW,
    .context = NULL,
    .internal = { 0 },

    .init = init,
    .start = start,
    .suspend = periodic_driver_suspend,
    .resume = resume,
    .stop = stop,
};
