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
#include "nvs_flash.h"

std::atomic<bool> relay_on = false;

static const char* TAG = "CAMCTL";

extern "C"
void app_main()
{
    init_gpio();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    xTaskCreate(led_task, "led_task", 4*1024, NULL, 2, NULL);
    
    xTaskCreate(gw_task, "gw_task", 4*1024, NULL, 1, NULL);

    set_led_pattern(RedBlink);

    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    int8_t value = 0;
    auto err = nvs_get_i8(my_handle, "relay", &value);
    switch (err)
    {
    case ESP_OK:
        ESP_LOGI(TAG, "Stored relay value: %d", value);
        relay_on.store(value);
        set_led_pattern(relay_on.load() ? SolidGreen : RedBlink);
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGI(TAG, "No stored relay value");
        break;
    default:
        printf("NVS error %d\n", err);
        break;
    }
    nvs_close(my_handle);

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
