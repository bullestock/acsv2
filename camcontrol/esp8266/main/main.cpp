// Camera control

#include "connect.h"
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
    
    xTaskCreate(led_task, "led_task", 4*1024, NULL, 5, NULL);
    
    set_led_pattern(RedFlash);

    init_wifi();
    if (!connect("hal9k"))
    {
        disconnect();
        connect("bullestock-guest");
    }

    xTaskCreate(gw_task, "gw_task", 4*1024, NULL, 5, NULL);
    set_led_pattern(BlueFlash);

    set_led_pattern(GreenBlink);
    while (1)
    {
        vTaskDelay(10 / portTICK_RATE_MS);
        const auto buttons = read_buttons();
        const auto old_relay_on = relay_on.load();
        auto new_relay_on = old_relay_on;
        if (buttons.first)
            new_relay_on = false;
        else if (buttons.second)
            new_relay_on = true;
        if (new_relay_on != old_relay_on)
        {
            set_led_pattern(new_relay_on ? SolidRed : GreenBlink);
            relay_on.store(new_relay_on);
        }
        set_relay(new_relay_on);
    }
}
