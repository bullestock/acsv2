#pragma once

#include "util.h"

#include <string>
#include <vector>

#include <TFT_eSPI.h>

class Display
{
public:
    Display(TFT_eSPI& tft);

    void start_uptime_counter();
    
    void clear();

    void update();

    /// Set the persistent status.
    void set_status(const std::string& status,
                    uint16_t colour = TFT_WHITE);

    /// \overload
    void set_status(const std::string& status,
                    uint16_t colour,
                    const std::string& aux_status,
                    uint16_t aux_colour);

    /// Add progress message (used during boot).
    void add_progress(const std::string& status);

    /// Show a message in the status area for MESSAGE_DURATION.
    void show_message(const std::string& message, uint16_t colour = TFT_WHITE);

private:
    void clear_status_area();
    
    void show_text(const std::string& status,
                   uint16_t colour,
                   const std::string& aux_status,
                   uint16_t aux_colour);

    TFT_eSPI& tft;
    int small_textheight = 0;
    int medium_textheight = 0;
    int large_textheight = 0;
    // Used by add_progress()
    int row = 0;
    std::vector<std::string> lines;
    // Used by set_status()
    std::string last_status;
    std::string last_aux_status;
    uint16_t last_status_colour = 0;
    uint16_t last_aux_status_colour = 0;
    // Used by show_message()
    util::time_point last_message = util::invalid_time_point();
    // Used by update()
    time_t last_clock = 0;
    time_t start_time = 0;
    int seconds_since_status_update = 59; // first update after 1 seconds
    int status_page = 0;
};
