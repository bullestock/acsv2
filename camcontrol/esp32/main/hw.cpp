#include "hw.h"
#include "defs.h"

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"

void init_gw()
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << RELAY1_PIN) |
        (1ULL << RELAY2_PIN);
    //disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    //disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BUTTON_PIN);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
}

void set_relay(bool on)
{
    gpio_set_level(RELAY_PIN, on);
}

bool read_button()
{
    return !gpio_get_level(BUTTON_PIN);
}
