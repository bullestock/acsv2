#include "gpio.h"

#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"

// D1
static const auto RELAY_PIN = (gpio_num_t) 5;

// D7
static const auto BUTTON_PIN = (gpio_num_t) 13;

// D3
static const auto LED_ONLINE_PIN = (gpio_num_t) 4;

// D2
static const auto LED_CAMERA_PIN = (gpio_num_t) 0;

void init_gpio()
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << RELAY_PIN) |
        (1ULL << LED_ONLINE_PIN) |
        (1ULL << LED_CAMERA_PIN);
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

void set_led_online(bool on)
{
    gpio_set_level(LED_ONLINE_PIN, on);
}

void set_led_camera(bool on)
{
    gpio_set_level(LED_CAMERA_PIN, on);
}

bool read_button()
{
    return !gpio_get_level(BUTTON_PIN);
}
