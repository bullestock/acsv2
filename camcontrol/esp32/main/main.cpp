// Camera control

#include <stdio.h>
#include <chrono>
#include <mutex>
#include <random>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_app_desc.h"
#include "esp_random.h"
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
#include "sntp.h"

#include <driver/i2c_master.h>

bool camera_relay_on = false;
bool estop_relay_on = false;

// Protects camera_relay_on and estop_relay_on
std::mutex relay_mutex;

static bool check_console(Display& display)
{
    display.add_progress("Wait for console");
    printf("\n\nPress a key to enter console\n");
    int keypresses = 0;
    bool debug = false;
    for (int i = 0; i < 20; ++i)
    {
        if (getchar() != EOF)
        {
            ++keypresses;
            display.add_progress("<key>");
            printf("<key>\n");
            if (keypresses > 5)
            {
                display.add_progress("Enter console");
                debug = true;
                break;
            }
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
            display.add_progress("SNTP synch");
            initialize_sntp();
#if 1
            // OTA check
            display.add_progress("OTA check");
            if (!check_ota_update(display))
                display.add_progress("FAILED!");
#endif
            xTaskCreate(gw_task, "gw_task", 4*1024, NULL, 1, NULL);
            xTaskCreate(logger_task, "log_task", 4*1024, NULL, 1, NULL);
        }
    }
    if (!debug)
        debug = check_console(display);

    Logger::instance().set_token(get_log_token());
    Logger::instance().set_log_to_gateway(true);
    
    if (debug)
        run_console(display);        // never returns

    printf("Starting app\n");
    
    display.clear();

    time_t start_time;
    time(&start_time);
    std::default_random_engine generator(esp_random()); // HW RNG seed
    std::uniform_int_distribution<int> distribution(10, 40);
    int reboot_minute = distribution(generator);
    
    camera_relay_on = estop_relay_on = get_relay_state();
    display.add_progress(format("Relay: %d", camera_relay_on));
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    display.clear();
    bool last_camera_relay_on = camera_relay_on;
    bool last_estop_relay_on = estop_relay_on;
    display.set_status(camera_relay_on, estop_relay_on);
    bool last_button = false;
    int debounce = 0;
    while (1)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        std::lock_guard<std::mutex> g(relay_mutex);
        const auto button = read_button();
        if (button != last_button)
        {
            if (++debounce > 5)
            {
                printf("Button toggled\n");
                debounce = 0;
                last_button = button;
                if (!button)
                {
                    camera_relay_on = !camera_relay_on;
                    estop_relay_on = camera_relay_on;
                    printf("Saving relay state %d\n", camera_relay_on);
                    set_relay_state(camera_relay_on);
                }
            }
        }
        if (camera_relay_on != last_camera_relay_on ||
            estop_relay_on != last_estop_relay_on)
        {
            display.set_status(camera_relay_on, estop_relay_on);
        }

        set_relay1(camera_relay_on);
        set_relay2(estop_relay_on);

        last_camera_relay_on = camera_relay_on;
        last_estop_relay_on = estop_relay_on;

        time_t current_time;
        time(&current_time);
        const auto since_start = current_time - start_time;
        if (since_start > 15*60)
        {
            struct tm tm;
            gmtime_r(&current_time, &tm);
            if (tm.tm_hour == 2 && tm.tm_min == reboot_minute)
            {
                Logger::instance().log("Scheduled reboot");
                display.set_status("Reboot", "in 60 s");
                vTaskDelay(60000 / portTICK_PERIOD_MS);
                esp_restart();
            }
        }
    }
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
