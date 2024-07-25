// Camera control

#include <stdio.h>
#include <chrono>
#include <random>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_app_desc.h"
#include "esp_wifi.h"

#include "connect.h"
#include "defs.h"
#include "display.h"
#include "format.h"
#include "gateway.h"
#include "hw.h"
#include "nvs.h"
#include "slack.h"

bool relay_on = false;

extern "C"
void app_main()
{
    init_hw();

    const auto app_desc = esp_app_get_description();
                                                  
    printf("ACS frontend v %s\n", app_desc->version);

    TFT_eSPI tft;
    Display display(tft);
    display.add_progress(format("ACS v %s", app_desc->version));

    display.add_progress("NVS init");

    init_nvs();

    xTaskCreate(gw_task, "gw_task", 4*1024, NULL, 1, NULL);

    /*
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
    */
    
    bool last_button = false;
    int debounce = 0;
    while (1)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        const auto button = read_button();
        auto is_relay_on = relay_on;
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
        relay_on = is_relay_on;
        set_relay(is_relay_on);
    }
}
