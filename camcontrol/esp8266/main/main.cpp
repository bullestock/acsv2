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
    Led the_led;
    connect();
    the_led.set_color(RgbColor(0, 255, 0));

    bool relay_on = false;
    while (1)
    {
        vTaskDelay(10 / portTICK_RATE_MS);
        const auto buttons = read_buttons();
        const auto old_relay_on = relay_on;
        if (buttons.first)
            relay_on = false;
        else if (buttons.second)
            relay_on = true;
        if (relay_on != old_relay_on)
            the_led.set_color(relay_on ? RgbColor(255, 0, 0) : RgbColor(0, 255, 0));
        set_relay(relay_on);
    }
}
