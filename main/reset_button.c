#include "reset_button.h"
#include <button.h>
#include "bus.h"

button_t btn = { 0 };

static void callback(button_t *b, button_state_t state)
{
    if (state != BUTTON_PRESSED_LONG) return;

    ESP_LOGW(TAG, "Reset button pressed!");
    bus_send_event(SETTINGS_RESET, NULL, 0);
}

esp_err_t reset_button_init()
{
    btn.gpio = HW_RESET_BUTTON_GPIO;
    btn.internal_pull = HW_RESET_BUTTON_PULLUPDOWN;
    btn.pressed_level = HW_RESET_BUTTON_LEVEL;
    btn.autorepeat = false;
    btn.callback = callback;

    return button_init(&btn);
}
