#include <stdio.h>
#include <string>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "connect.h"
#include "console.h"
#include "defs.h"
#include "display.h"
#include "format.h"
#include "hw.h"
#include "gateway.h"
#include "rs485.h"

using Thresholds = std::vector<std::pair<float, uint16_t>>;

void fatal_error(TFT_eSPI& tft, const std::string& error)
{
    printf("FATAL: %s\n", error.c_str());
    const auto msg = "ERROR:\n" + error;
    set_status(tft, msg, TFT_RED);
    set_relay(false);
    
    while (1)
    {
        vTaskDelay(10/portTICK_PERIOD_MS);
    }
}

extern "C"
void app_main()
{
    init_hardware();
    init_rs485();

    printf("ACS frontend v %s\n", VERSION);

    TFT_eSPI tft;
    init(tft);

    set_status(tft, "NVS init");
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    set_status(tft, "Connect to WiFi");

    ESP_ERROR_CHECK(connect({ "bullestock-guest" /*, "hal9k" */ }));
    ESP_LOGI(TAG, "Connected to WiFi");
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    set_status(tft, "Connected");
    
    xTaskCreate(gw_task, "gw_task", 4*1024, NULL, 1, NULL);
    
    printf("\n\nPress a key to enter console\n");
    bool debug = false;
    for (int i = 0; i < 20; ++i)
    {
        if (getchar() != EOF)
        {
            debug = true;
            break;
        }
        vTaskDelay(100/portTICK_PERIOD_MS);
    }
    if (debug)
        run_console();        // never returns

    printf("\nStarting application\n");

    //uint64_t last_tick = 0;
    while (1)
    {
        vTaskDelay(10);
#if 0
        const auto now = esp_timer_get_time(); // microseconds
        const auto elapsed = (now - last_tick) / 1000;
        if (elapsed > 1000)
        {
            const auto cmd = "S1000 100\n";
            write_rs485(cmd, strlen(cmd));
            last_tick = now;
            printf("Wrote %s", cmd);
        }
        char buf[80];
        const auto read = read_rs485(buf, sizeof(buf));
        buf[read] = 0;
        printf("Read %d: %s\n", read, buf);
#endif
    }
}
