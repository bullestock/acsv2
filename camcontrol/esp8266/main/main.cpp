// Camera control

#include "gateway.h"
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

std::atomic<bool> relay_on = false;

extern "C"
void app_main()
{
    init_gpio();
    
    xTaskCreate(led_task, "led_task", 4*1024, NULL, 2, NULL);
    
    xTaskCreate(gw_task, "gw_task", 4*1024, NULL, 1, NULL);

    set_led_pattern(RedBlink);

    while (1)
    {
        vTaskDelay(10 / portTICK_RATE_MS);
        const auto buttons = read_buttons();
        auto is_relay_on = relay_on.load();
        const auto old_on = is_relay_on;
        if (buttons.first)
            is_relay_on = false;
        else if (buttons.second)
            is_relay_on = true;
        if (is_relay_on != old_on)
            set_led_pattern(is_relay_on ? SolidGreen : RedBlink);
        relay_on.store(is_relay_on);
        set_relay(is_relay_on);
    }
}
