#include "gpio.h"

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"

// D1
static const auto RELAY_PIN = (gpio_num_t) 5;

// D4
static const auto RED_PIN = (gpio_num_t) 2;

// D3
static const auto GREEN_PIN = (gpio_num_t) 0;

void init_gpio()
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = 1ULL << RELAY_PIN;
    //disable pull-down mode
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    //disable pull-up mode
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << RED_PIN) | (1ULL << GREEN_PIN);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
}

void set_relay(bool on)
{
    gpio_set_level(RELAY_PIN, on);
}

std::pair<bool, bool> read_buttons()
{
    return std::make_pair<bool, bool>(!gpio_get_level(RED_PIN),
                                      !gpio_get_level(GREEN_PIN));
}
