#include "gh_io.h"

#ifdef DRIVER_GH_IO

#include <esp_log.h>
#include <esp_check.h>
#include "settings.h"
#include <tca95x5.h>

#define FMT_RELAY_ID    "relay%d"
#define FMT_INPUT_ID    "input%d"
#define FMT_SWITCH_ID   "switch%d"
#define FMT_LED_ID      "led%d"

#define FMT_RELAY_NAME  "%s relay %d"
#define FMT_INPUT_NAME  "%s isolated input %d"
#define FMT_SWITCH_NAME "%s input switch %d"
#define FMT_LED_NAME    "%s LED %d"

#define SWITCHES_COUNT 4
#define INPUTS_COUNT 4
#define PORT_MODE 0xff00 // low 8 bits = input, high 8 bits = output

#define CHANGED_BITS BIT(0)

static i2c_dev_t expander = { 0 };
static EventGroupHandle_t port_event = NULL;
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
        ESP_LOGE(self->name, "Error creating event group");
        return ESP_ERR_NO_MEM;
    }

    memset(&expander, 0, sizeof(expander));
    ESP_RETURN_ON_ERROR(
        tca95x5_init_desc(&expander, DRIVER_GH_IO_ADDRESS, HW_INTERNAL_PORT, HW_INTERNAL_SDA_GPIO, HW_INTERNAL_SCL_GPIO),
        self->name, "Error initializing device descriptor: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );
#if (DRIVER_GH_IO_FREQUENCY)
    expander.cfg.master.clk_speed = DRIVER_GH_IO_FREQUENCY;
#endif
    ESP_RETURN_ON_ERROR(
        tca95x5_port_write(&expander, 0),
        self->name, "Error writing to port: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );
    ESP_RETURN_ON_ERROR(
        tca95x5_port_set_mode(&expander, PORT_MODE),
        self->name, "Error setting up port mode: %d (%s)", err_rc_, esp_err_to_name(err_rc_)
    );

    ESP_LOGI(self->name, "Initialized TCA9555: ADDR=0x%02x, PORT=%d, SDA=%d, SCL=%d, FREQ=%d",
        DRIVER_GH_IO_ADDRESS, HW_INTERNAL_PORT, HW_INTERNAL_SDA_GPIO, HW_INTERNAL_SCL_GPIO, DRIVER_GH_IO_FREQUENCY);

    ESP_RETURN_ON_ERROR(
        gpio_reset_pin(DRIVER_GH_IO_INTR_GPIO),
        self->name, "Error reset INTR GPIO %d: %d (%s)", DRIVER_GH_IO_INTR_GPIO, err_rc_, esp_err_to_name(err_rc_)
    );
    gpio_set_direction(DRIVER_GH_IO_INTR_GPIO, GPIO_MODE_INPUT);
    gpio_set_intr_type(DRIVER_GH_IO_INTR_GPIO, GPIO_INTR_NEGEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(DRIVER_GH_IO_INTR_GPIO, on_port_change, NULL);

    device_t dev;

    for (uint32_t i = 0; i < DRIVER_GH_IO_RELAY_COUNT; i++)
    {
        memset(&dev, 0, sizeof(dev));
        dev.type = DEV_BINARY_SWITCH;
        dev.internal[0] = (void *)i;
        dev.binary_switch.on_write = on_relay_command;
        snprintf(dev.uid, sizeof(dev.uid), FMT_RELAY_ID, (int)i);
        snprintf(dev.name, sizeof(dev.name), FMT_RELAY_NAME, settings.system.name, (int)i);
        cvector_push_back(self->devices, dev);
    }

    inputs = self->devices + DRIVER_GH_IO_RELAY_COUNT;
    for (int i = 0; i < INPUTS_COUNT; i++)
    {
        memset(&dev, 0, sizeof(dev));
        dev.type = DEV_BINARY_SENSOR;
        snprintf(dev.uid, sizeof(dev.uid), FMT_INPUT_ID, i);
        snprintf(dev.name, sizeof(dev.name), FMT_INPUT_NAME, settings.system.name, i);
        cvector_push_back(self->devices, dev);
    }

    switches = inputs + INPUTS_COUNT;
    for (int i = 0; i < SWITCHES_COUNT; i++)
    {
        memset(&dev, 0, sizeof(dev));
        dev.type = DEV_BINARY_SENSOR;
        snprintf(dev.uid, sizeof(dev.uid), FMT_SWITCH_ID, i);
        snprintf(dev.name, sizeof(dev.name), FMT_SWITCH_NAME, settings.system.name, i);
        cvector_push_back(self->devices, dev);
    }

#if DRIVER_GH_IO_LED0_PIN
    memset(&dev, 0, sizeof(dev));
    dev.type = DEV_BINARY_SWITCH;
    dev.internal[0] = (void *)DRIVER_GH_IO_LED0_PIN;
    dev.binary_switch.on_write = on_relay_command;
    snprintf(dev.uid, sizeof(dev.uid), FMT_LED_ID, 0);
    snprintf(dev.name, sizeof(dev.name), FMT_LED_NAME, settings.system.name, 0);
    cvector_push_back(self->devices, dev);
#endif

#if DRIVER_GH_IO_LED1_PIN
    memset(&dev, 0, sizeof(dev));
    dev.type = DEV_BINARY_SWITCH;
    dev.internal[0] = (void *)DRIVER_GH_IO_LED1_PIN;
    dev.binary_switch.on_write = on_relay_command;
    snprintf(dev.uid, sizeof(dev.uid), FMT_LED_ID, 1);
    snprintf(dev.name, sizeof(dev.name), FMT_LED_NAME, settings.system.name, 1);
    cvector_push_back(self->devices, dev);
#endif

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
    gpio_isr_handler_remove(DRIVER_GH_IO_INTR_GPIO);
    vEventGroupDelete(port_event);
    port_event = NULL;
    esp_err_t r = tca95x5_free_desc(&expander);
    if (r != ESP_OK)
        ESP_LOGW(self->name, "Device descriptor free error: %d (%s)", r, esp_err_to_name(r));

    return ESP_OK;
}

driver_t drv_gh_io = {
    .name = "gh_io",
    .stack_size = DRIVER_GH_IO_STACK_SIZE,
    .priority = tskIDLE_PRIORITY + 1,
    .defconfig = NULL,

    .config = NULL,
    .state = DRIVER_NEW,
    .event_queue = NULL,

    .devices = NULL,
    .handle = NULL,
    .eg = NULL,

    .on_init = on_init,
    .on_start = NULL,
    .on_stop = on_stop,

    .task = task
};

#endif
