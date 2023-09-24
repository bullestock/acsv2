#pragma once

#include "util.h"

#include <string>
#include <vector>

#include <TFT_eSPI.h>

class Display
{
public:
    Display(TFT_eSPI& tft);

    void clear();

    void update();

    void set_status(const std::string& status,
                    uint16_t colour = TFT_WHITE, bool large = false);

    void add_progress(const std::string& status);

    void show_message(const std::string& message, uint16_t colour = TFT_WHITE);

private:
    void clear_status_area();
    
    void show_text(const std::string& status, uint16_t colour, bool large);

    TFT_eSPI& tft;
    int small_textheight = 0;
    int medium_textheight = 0;
    int large_textheight = 0;
    // Used by add_progress()
    int row = 0;
    std::vector<std::string> lines;
    // Used by set_status()
    std::string last_status;
    uint16_t last_status_colour = 0;
    bool last_status_large = false;
    // Used by show_message()
    util::time_point last_message = util::invalid_time_point();
    // Used by update()
    time_t last_clock = 0;
    int clock_x = 0;
    uint64_t uptime = 0;
};
