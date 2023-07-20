// Camera control

#include "gateway.h"
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

bool relay_on = false;

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

#if 0
    xTaskCreate(led_task, "led_task", 4*1024, NULL, 2, NULL);
#endif

    xTaskCreate(gw_task, "gw_task", 4*1024, NULL, 1, NULL);

    set_led_online(false);
    set_led_camera(false);

    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    int8_t value = 0;
    auto err = nvs_get_i8(my_handle, "relay", &value);
    switch (err)
    {
    case ESP_OK:
        printf("Stored relay value: %d\n", value);
        relay_on = value;
        set_led_camera(relay_on);
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        printf(TAG, "No stored relay value\n");
        break;
    default:
        printf("NVS error %d\n", err);
        break;
    }
    nvs_close(my_handle);

    bool last_button = false;
    int debounce = 0;
    while (1)
    {
        vTaskDelay(10 / portTICK_RATE_MS);
        const auto button = read_button();
        auto is_relay_on = relay_on;
        const auto old_on = is_relay_on;
        if (button != last_button)
        {
            if (++debounce > 5)
            {
                printf("Button toggled\n");
                debounce = 0;
                last_button = button;
                if (!button)
                    is_relay_on = !is_relay_on;
            }
        }
        if (is_relay_on != old_on)
            set_led_camera(is_relay_on);
        relay_on = is_relay_on;
        set_relay(is_relay_on);
    }
}
