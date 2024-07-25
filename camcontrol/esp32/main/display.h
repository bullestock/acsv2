#pragma once

#include <string>
#include <vector>

#include <TFT_eSPI.h>

class Display
{
public:
    Display(TFT_eSPI& tft);

    void clear();

    /// Set the persistent status.
    void set_status(const std::string& status,
                    uint16_t colour = TFT_WHITE, bool large = false);

    /// Add progress message (used during boot).
    void add_progress(const std::string& status);

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
};
