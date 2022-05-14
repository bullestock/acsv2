// Camera control

#include <stdio.h>
#include <chrono>
#include <random>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_timer.h"
#include "driver/gpio.h"

// D1
static const auto RELAY_PIN = (gpio_num_t) 5;

extern "C"
void app_main()
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
    while (1)
    {
        vTaskDelay(1000 / portTICK_RATE_MS);
        printf("on\n");
        gpio_set_level(RELAY_PIN, 1);
        vTaskDelay(1000 / portTICK_RATE_MS);
        printf("off\n");
        gpio_set_level(RELAY_PIN, 0);
    }
}
