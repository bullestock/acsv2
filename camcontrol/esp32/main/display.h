#pragma once

#include <string>
#include <vector>

#include "ssd1306.h"

class Display
{
public:
    Display(SSD1306_t& d);

    void clear();

    SSD1306_t* device();

    /// Add progress message (used during boot).
    void add_progress(const std::string& status);
    
private:
    SSD1306_t& display;
};
