#include "gh_dimmer.h"
#include <esp_log.h>
#include <esp_check.h>
#include <ets_sys.h>
#include "settings.h"
#include "common.h"

#define FMT_DIMMER_ID    "dimmer"
#define FMT_DIMMER_NAME  "%s dimmer"

#define RELAYS_COUNT 7
#define SWITCHES_COUNT 4
#define INPUTS_COUNT 4
#define PORT_MODE 0xff00 // low 8 bits = input, high 8 bits = output

#define ZERO_CROSSING_BIT BIT(0)

static EventGroupHandle_t zc_group = NULL;
static int ac_freq = 0;

static void IRAM_ATTR on_zero_cross(void *arg)
{
    (void)arg;
    BaseType_t hp_task;
    if (xEventGroupSetBitsFromISR(zc_group, ZERO_CROSSING_BIT, &hp_task) != pdFAIL)
        portYIELD_FROM_ISR(hp_task);
}

static void on_value_change(device_t *dev, float val)
{
    dev->number.value = val;
    driver_send_device_update(&drv_gh_dimmer, dev);
}

static esp_err_t on_init(driver_t *self)
{
    cvector_free(self->devices);

    if (zc_group)
    {
        vEventGroupDelete(zc_group);
        zc_group = NULL;
    }
    zc_group = xEventGroupCreate();
    if (!zc_group)
    {
        ESP_LOGE(self->name, "Error creating AC Zero Crossing group");
        return ESP_ERR_NO_MEM;
    }

    ac_freq = driver_config_get_int(cJSON_GetObjectItem(self->config, OPT_FREQ), 50);
    ESP_LOGI(self->name, "AC frequency: %d Hz", ac_freq);

    gpio_reset_pin(DRIVER_GH_DIMMER_ZERO_GPIO);
    gpio_set_direction(DRIVER_GH_DIMMER_ZERO_GPIO, GPIO_MODE_INPUT);
    gpio_set_intr_type(DRIVER_GH_DIMMER_ZERO_GPIO, GPIO_INTR_POSEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(DRIVER_GH_DIMMER_ZERO_GPIO, on_zero_cross, NULL);

    gpio_reset_pin(DRIVER_GH_DIMMER_CTRL_GPIO);
    gpio_set_direction(DRIVER_GH_DIMMER_CTRL_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(DRIVER_GH_DIMMER_CTRL_GPIO, 0);

    device_t dev = { 0 };
    dev.type = DEV_NUMBER;
    strncpy(dev.uid, FMT_DIMMER_ID, sizeof(dev.uid));
    snprintf(dev.name, sizeof(dev.name), FMT_DIMMER_NAME, settings.node.name);
    dev.number.measurement_unit[0] = '%';
    dev.number.min = 0.0f;
    dev.number.max = 100.0f;
    dev.number.step = 1.0f;
    dev.number.value = 50.0f;
    dev.number.on_write = on_value_change;

    cvector_push_back(self->devices, dev);

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
        while (!(xEventGroupGetBits(zc_group) & ZERO_CROSSING_BIT))
        {
            if (!(xEventGroupGetBits(self->eg) & DRIVER_BIT_START))
                return;
            vTaskDelay(1);
        }
        xEventGroupClearBits(zc_group, ZERO_CROSSING_BIT);

        uint32_t delay = (100 - (int)self->devices[0].number.value) * (1000000 / ac_freq / 100);
        ets_delay_us(delay);
        gpio_set_level(DRIVER_GH_DIMMER_CTRL_GPIO, 1);
        ets_delay_us(10);
        gpio_set_level(DRIVER_GH_DIMMER_CTRL_GPIO, 0);
    }
}

static esp_err_t on_stop(driver_t *self)
{
    gpio_isr_handler_remove(DRIVER_GH_DIMMER_ZERO_GPIO);
    vEventGroupDelete(zc_group);
    zc_group = NULL;

    return ESP_OK;
}

driver_t drv_gh_dimmer = {
    .name = "gh_dimmer",
    .stack_size = DRIVER_GH_DIMMER_STACK_SIZE,
    .priority = tskIDLE_PRIORITY + 1,
    .defconfig = "{ \"" OPT_FREQ "\": 50 }",

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
