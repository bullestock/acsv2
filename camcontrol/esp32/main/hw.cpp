#include "hw.h"
#include "defs.h"

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"

void init_hardware()
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_RELAY1) |
        (1ULL << PIN_RELAY2);
    //disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    //disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_BUTTON);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
}

void set_relay1(bool on)
{
    gpio_set_level(PIN_RELAY1, on);
}

void set_relay2(bool on)
{
    gpio_set_level(PIN_RELAY2, on);
}

bool read_button()
{
    return !gpio_get_level(PIN_BUTTON);
}
