/* SPDX-License-Identifier: Apache-2.0 */
#include "board_button.h"
#include <stdatomic.h>
#include "iot_button.h"
#include "button_gpio.h"

static button_handle_t boot_button;
static _Atomic uint32_t press_count;
static void boot_pressed(void *button, void *context)
{
    (void)button;
    (void)context;
    atomic_fetch_add_explicit(&press_count, 1, memory_order_relaxed);
}
esp_err_t board_user_button_init(void)
{
    if (boot_button) return ESP_OK;
    const button_config_t config = {0};
    // Same active-low GPIO35 as examples/esp-idf/12_esp32-p4-eye.
    const button_gpio_config_t gpio = {
        .gpio_num = GPIO_NUM_35,
        .active_level = 0,
        .enable_power_save = false,
        .disable_pull = false,
    };
    esp_err_t ret = iot_button_new_gpio_device(&config, &gpio, &boot_button);
    if (ret != ESP_OK) return ret;
    // Debounced press-down fires once per press, including a held button.
    ret = iot_button_register_cb(boot_button, BUTTON_PRESS_DOWN, NULL, boot_pressed, NULL);
    if (ret != ESP_OK) {
        iot_button_delete(boot_button);
        boot_button = NULL;
    }
    return ret;
}
uint32_t board_user_button_get_press_count(void)
{
    return atomic_load_explicit(&press_count, memory_order_relaxed);
}
