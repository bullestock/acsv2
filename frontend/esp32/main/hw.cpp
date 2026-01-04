#include "hw.h"
#include "defs.h"
#include "util.h"

#include <limits>
#include <vector>

#include "esp_log.h"

#include <freertos/FreeRTOS.h>
#include <driver/ledc.h>

void init_hardware()
{
    // Inputs
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    // bit mask of the pins that you want to set
    io_conf.pin_bit_mask = (1ULL << PIN_DISP_SDI) |
        (1ULL << PIN_RED) |
        (1ULL << PIN_WHITE) |
        (1ULL << PIN_GREEN) |
        (1ULL << PIN_LEAVE) |
        (1ULL << PIN_DOOR);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    io_conf.pin_bit_mask = (1ULL << PIN_EXT_1) | (1ULL << PIN_EXT_2);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Outputs
    io_conf.mode = GPIO_MODE_OUTPUT;
    // bit mask of the pins that you want to set
    io_conf.pin_bit_mask = (1ULL << PIN_DISP_DC) |
       (1ULL << PIN_DISP_RESET) |
       (1ULL << PIN_DISP_CS) |
       (1ULL << PIN_DISP_SDO) |
       (1ULL << PIN_RELAY);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&io_conf));

}

void set_relay(bool on)
{
    ESP_ERROR_CHECK(gpio_set_level(PIN_RELAY, on));
}

bool get_door_open()
{
    return gpio_get_level(PIN_DOOR);
}

