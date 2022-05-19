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
    xTaskCreate(led_task, "led_task", 4*1024, NULL, 5, NULL);

    connect();
    set_led_pattern(BlueFlash);

    bool relay_on = false;
    set_led_pattern(GreenBlink);
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
            set_led_pattern(relay_on ? SolidRed : GreenBlink);
        set_relay(relay_on);
    }
}
