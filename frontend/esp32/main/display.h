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

    void set_status(const std::string& status,
                    uint16_t colour = TFT_WHITE, bool large = false);

    void add_progress(const std::string& status);

    void show_message(const std::string& message, uint16_t colour = TFT_WHITE);

private:
    TFT_eSPI& tft;
    int textheight = 0;
    // Used by add_progress()
    int row = 0;
    std::vector<std::string> lines;
    // Used by set_status()
    std::string last_status;
    // Used by show_message()
    util::time_point last_message = util::invalid_time_point();
};
