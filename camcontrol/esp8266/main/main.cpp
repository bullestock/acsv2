// Camera control

#include "connect.h"
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
    connect();
    
    while (1)
    {
        vTaskDelay(1000 / portTICK_RATE_MS);
        printf("on\n");
        set_relay(1);
        vTaskDelay(1000 / portTICK_RATE_MS);
        printf("off\n");
        set_relay(0);
    }
}
