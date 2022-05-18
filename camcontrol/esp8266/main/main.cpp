// Camera control

#include "connect.h"
#include "led.h"
#include "gpio.h"

#include <stdio.h>
#include <chrono>
#include <random>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_timer.h"

extern "C"
void app_main()
{
    init_gpio();
    the_led.set_color(RgbColor(0, 0, 128));
    connect();
    the_led.set_color(RgbColor(0, 255, 0));
    
    while (1)
    {
        vTaskDelay(1000 / portTICK_RATE_MS);
        printf("on\n");
        the_led.set_color(RgbColor(255, 0, 0));
        set_relay(1);
        vTaskDelay(1000 / portTICK_RATE_MS);
        printf("off\n");
        the_led.set_color(RgbColor(0, 255, 128));
        set_relay(0);
        const auto buttons = read_buttons();
        printf("R %d G %d\n", buttons.first, buttons.second);
    }
}
