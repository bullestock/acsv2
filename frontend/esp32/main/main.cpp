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
#include "foreninglet.h"
#include "format.h"
#include "gateway.h"
#include "hw.h"
#include "logger.h"
#include "nvs.h"
#include "rs485.h"
#include "slack.h"
#include "sntp.h"

using Thresholds = std::vector<std::pair<float, uint16_t>>;

void fatal_error(Display& display, const std::string& error)
{
    printf("FATAL: %s\n", error.c_str());
    const auto msg = "ERROR:\n" + error;
    display.set_status(msg, TFT_RED);
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
    Display display(tft);
    display.add_progress(format("ACS v %s", VERSION));

    display.add_progress("NVS init");

    init_nvs();

    const auto wifi_creds = get_wifi_creds();
    if (!wifi_creds.empty())
    {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());

        display.add_progress("Connect to WiFi");

        if (connect(wifi_creds))
        {
            ESP_LOGI(TAG, "Connected to WiFi");
            ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

            display.add_progress("SNTP synch");

            initialize_sntp();
            
            display.add_progress("Connected");

            printf("\nConnected to WiFi\n");
        }
    }
    
    Gateway::instance().set_token(get_gateway_token());
    xTaskCreate(gw_task, "gw_task", 4*1024, NULL, 1, NULL);
    Logger::instance().set_api_token(get_acs_token());
    Logger::instance().set_gateway_token(get_gateway_token());
    Card_cache::instance().set_api_token(get_acs_token());
    xTaskCreate(logger_task, "logger_task", 4*1024, NULL, 1, NULL);
    xTaskCreate(card_reader_task, "cr_task", 4*1024, NULL, 1, NULL);
    xTaskCreate(card_cache_task, "cache_task", 4*1024, NULL, 1, NULL);
    Slack_writer::instance().set_token(get_slack_token());
    Slack_writer::instance().set_params(false); // testing
    ForeningLet::instance().set_credentials(get_foreninglet_username(),
                                            get_foreninglet_password());
    xTaskCreate(foreninglet_task, "fl_task", 4*1024, NULL, 1, NULL);
    
    display.add_progress("Wait for console");

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

    Slack_writer::instance().send_message(format(":frontend: ACS frontend %s", VERSION));

    printf("\nStarting application\n");
    display.add_progress("Starting");

    Controller controller(display, Card_reader::instance());
    display.clear();
    controller.run(); // never returns
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
