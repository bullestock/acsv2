// Camera control

#include <stdio.h>
#include <chrono>
#include <random>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_app_desc.h"
#include "esp_wifi.h"

#include "connect.h"
#include "console.h"
#include "defs.h"
#include "display.h"
#include "format.h"
#include "gateway.h"
#include "hw.h"
#include "logger.h"
#include "nvs.h"
#include "otafwu.h"
#include "slack.h"

bool relay_on = false;

#include <driver/i2c_master.h>

static bool check_console(Display& display)
{
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
    return debug;
}

extern "C"
void app_main()
{
    init_hardware();

    const auto app_desc = esp_app_get_description();
                                                  
    printf("camctl %s\n", app_desc->version);

    SSD1306_t ssd;
    Display display(ssd);

    display.add_progress(format("camctl %s", app_desc->version));

    display.add_progress("NVS init");

    init_nvs();

    bool debug = false;
    const auto creds = get_wifi_creds();
    if (!creds.empty())
    {
        display.add_progress("WiFi connect");
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        if (!connect(creds))
        {
            display.add_progress("FAILED");
            debug = check_console(display);
            if (!debug)
                esp_restart();
        }
        else
        {
            // OTA check
            display.add_progress("OTA check");
            if (!check_ota_update(display))
                display.add_progress("FAILED!");
            
            xTaskCreate(gw_task, "gw_task", 4*1024, NULL, 1, NULL);
            xTaskCreate(slack_task, "slack_task", 4*1024, NULL, 1, NULL);
        }
    }
    if (!debug)
        debug = check_console(display);

    Logger::instance().set_gateway_token(get_gateway_token());
    Slack_writer::instance().set_token(get_slack_token());
    Slack_writer::instance().set_params(false); // testing
    
    if (debug)
        run_console(display);        // never returns

    printf("Starting app\n");
    
    display.clear();

    bool relay_on = get_relay1_state();
    display.set_status(relay_on ? "  On" : " Off");                    
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
                {
                    is_relay_on = !is_relay_on;
                    set_relay1_state(is_relay_on);
                }
            }
        }
        if (relay_on != is_relay_on)
            display.set_status(is_relay_on ? "  On" : " Off");                    

        relay_on = is_relay_on;
        set_relay1(is_relay_on);
    }
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
