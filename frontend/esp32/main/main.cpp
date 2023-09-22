#include <stdio.h>
#include <string>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_wifi.h"

#include "cardreader.h"
#include "connect.h"
#include "console.h"
#include "controller.h"
#include "defs.h"
#include "display.h"
#include "format.h"
#include "gateway.h"
#include "hw.h"
#include "logger.h"
#include "nvs.h"
#include "rs485.h"
#include "slack.h"
#include "sntp.h"

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
    add_progress(tft, format("ACS v %s", VERSION));

    add_progress(tft, "NVS init");

    init_nvs();

    const auto wifi_creds = get_wifi_creds();
    if (!wifi_creds.empty())
    {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());

        add_progress(tft, "Connect to WiFi");

        if (connect(wifi_creds))
        {
            ESP_LOGI(TAG, "Connected to WiFi");
            ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

            add_progress(tft, "SNTP synch");

            initialize_sntp();
            
            add_progress(tft, "Connected");

            Gateway::instance().set_token(get_gateway_token());
            xTaskCreate(gw_task, "gw_task", 4*1024, NULL, 1, NULL);
            Logger::instance().set_api_token(get_acs_token());
            Logger::instance().set_gateway_token(get_gateway_token());
            Card_cache::instance().set_api_token(get_acs_token());
            xTaskCreate(logger_task, "logger_task", 4*1024, NULL, 1, NULL);
            xTaskCreate(card_reader_task, "cr_task", 4*1024, NULL, 1, NULL);
            printf("\nConnected to WiFi\n");
        }
    }
    
    Slack_writer::instance().set_token(get_slack_token());
    Slack_writer::instance().set_params(true, true);
    
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

    //Controller controller;
    
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
