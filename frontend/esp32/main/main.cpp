#include <stdio.h>
#include <string>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_app_desc.h"
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
#include "otafwu.h"
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

    const auto app_desc = esp_app_get_description();
                                                  
    printf("ACS frontend v %s\n", app_desc->version);

    TFT_eSPI tft;
    Display display(tft);
    display.add_progress(format("ACS v %s", app_desc->version));

    display.add_progress("NVS init");

    init_nvs();

    display.add_progress(format("ID %s", get_identifier().c_str()));

    // Turn on/off beta program if green/red pressed during boot
    auto buttons = Buttons::read();
    if (buttons.red || buttons.green)
    {
        const auto initial = buttons;
        for (int i = 0; i < 10; ++i)
        {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            buttons = Buttons::read();
            if (buttons != initial)
                break;
        }
        if (buttons == initial)
        {
            // Button state unchanged after 1 second
            if (buttons.green)
            {
                display.add_progress("Enter beta program");
                set_beta_program_active(true);
            }
            else if (buttons.red)
            {
                display.add_progress("Exit beta program");
                set_beta_program_active(false);
            }
        }
    }                

    bool connected = false;
    const auto wifi_creds = get_wifi_creds();
    if (!wifi_creds.empty())
    {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());

        display.add_progress("Connect to WiFi");

        int attempts_left = 5;
        while (!connected && attempts_left)
        {
            connected = connect(wifi_creds);
            ESP_LOGI(TAG, "Connected: %d attempts: %d",
                     connected, attempts_left);
            if (!connected)
            {
                disconnect();
                vTaskDelay(10000 / portTICK_PERIOD_MS);
                --attempts_left;
            }
        }
    }
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
    if (connected)
    {
        ESP_LOGI(TAG, "Connected to WiFi");
        ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

        display.add_progress("SNTP synch");

        initialize_sntp();
            
        display.add_progress("Connected");

        Gateway::instance().set_token(get_gateway_token());
        Logger::instance().set_api_token(get_acs_token());
        Logger::instance().set_gateway_token(get_gateway_token());
        Slack_writer::instance().set_token(get_slack_token());
        Slack_writer::instance().set_params(false); // testing
        if (!debug)
        {
            ESP_LOGI(TAG, "Start coredump upload");
            Gateway::instance().upload_coredump(display);
            ESP_LOGI(TAG, "Finished coredump upload");
        }
        Card_cache::instance().set_api_token(get_acs_token());
        xTaskCreate(gw_task, "gw_task", 4*1024, NULL, 1, NULL);
        xTaskCreate(logger_task, "logger_task", 4*1024, NULL, 1, NULL);
        xTaskCreate(card_cache_task, "cache_task", 4*1024, NULL, 1, NULL);
        ForeningLet::instance().set_credentials(get_foreninglet_username(),
                                                get_foreninglet_password());
        xTaskCreate(slack_task, "slack_task", 4*1024, NULL, 1, NULL);
        xTaskCreate(foreninglet_task, "fl_task", 4*1024, NULL, 1, NULL);
    }
    
    xTaskCreate(card_reader_task, "cr_task", 4*1024, NULL, 1, NULL);
    
    if (debug)
        run_console(display);        // never returns

    Slack_writer::instance().send_message(format(":frontend: ACS frontend %s (%s)",
                                                 app_desc->version, get_identifier().c_str()));

    printf("\nStarting application\n");
    display.start_uptime_counter();
    if (connected)
    {
        display.add_progress("OTA check");
        if (0 && !check_ota_update(display))
            display.add_progress("FAILED!");
    }
    esp_log_level_set("esp_wifi", ESP_LOG_ERROR);
    esp_log_level_set("wifi", ESP_LOG_ERROR);
    esp_log_level_set("HTTP_CLIENT", ESP_LOG_DEBUG);
    display.add_progress("Starting");
    Logger::instance().set_log_to_gateway(true);
    Logger::instance().log(format("ACS frontend %s (%s)",
                                  app_desc->version, get_identifier().c_str()));

    Controller controller(display, Card_reader::instance());
    display.clear();
    controller.run(); // never returns
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
