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

#include <driver/i2c_master.h>

extern "C"
void app_main()
{
    init_hardware();

    const auto app_desc = esp_app_get_description();
                                                  
    printf("Camcontrol v %s\n", app_desc->version);

#define I2C_TOOL_TIMEOUT_VALUE_MS (50)

    extern i2c_master_bus_handle_t i2c_bus_handle;

    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
    for (int i = 0; i < 128; i += 16) {
        printf("%02x: ", i);
        for (int j = 0; j < 16; j++) {
            fflush(stdout);
            uint8_t address = i + j;
            esp_err_t ret = i2c_master_probe(i2c_bus_handle, address, I2C_TOOL_TIMEOUT_VALUE_MS);
            if (ret == ESP_OK) {
                printf("%02x ", address);
            } else if (ret == ESP_ERR_TIMEOUT) {
                printf("UU ");
            } else {
                printf("-- ");
            }
        }
        printf("\r\n");
    }
#if 1
    SSD1306_t ssd;
    Display display(ssd);

    display.add_progress(format("CamControl v %s", app_desc->version));

    display.add_progress("NVS init");
#endif
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
        set_relay1(is_relay_on);
    }
}
