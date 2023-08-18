#include <stdio.h>
#include <string>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "console.h"
#include "defs.h"
#include "hw.h"
#include "rs485.h"

#include <TFT_eSPI.h>

using Thresholds = std::vector<std::pair<float, uint16_t>>;

static constexpr const auto small_font = &FreeSans12pt7b;
static constexpr const auto medium_font = &FreeSansBold18pt7b;
static constexpr const auto large_font = &FreeSansBold24pt7b;
static constexpr const int GFXFF = 1;

void set_status(TFT_eSPI& tft, const std::string& status, uint16_t colour = TFT_WHITE)
{
    static std::string last_status;

    if (status != last_status)
    {
        tft.fillRect(TFT_HEIGHT/2, TFT_WIDTH/2, TFT_HEIGHT/2, TFT_WIDTH/2, TFT_BLACK);
        last_status = status;
    }
    tft.setTextColor(colour);
    tft.setFreeFont(large_font);
    const auto w = tft.textWidth(status.c_str(), GFXFF);
    const auto x = TFT_HEIGHT/2 + TFT_HEIGHT/4 - w/2;
    const auto y = TFT_HEIGHT/2 - 20;
    tft.drawString(status.c_str(), x, y, GFXFF);
}

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

    TFT_eSPI tft;
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setFreeFont(small_font);
    tft.setTextColor(TFT_CYAN);
    tft.drawString("ACS", TFT_HEIGHT/2, TFT_WIDTH/2, GFXFF);
    
    printf("\nStarting application\n");

    uint64_t last_tick = 0;
    while (1)
    {
        vTaskDelay(10);
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
    }
}
