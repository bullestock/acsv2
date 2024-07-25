#include "display.h"
#include "defs.h"

Display::Display(SSD1306_t& d)
    : display(d)
{
    display._address = DISPLAY_I2C_ADDRESS;
    display._flip = false;

    ssd1306_init(&display, 128, 64);
    ssd1306_contrast(&display, 0xff);
    clear();
}

void Display::clear()
{
    ssd1306_clear_screen(&display, false);
}

SSD1306_t* Display::device()
{
    return &display;
}

void Display::add_progress(const std::string& status)
{
    ssd1306_display_text(device(), row, status.c_str(), status.size(), false);
    ++row;
    lines.push_back(status);
    if (row < 8) //!!
        return; // still room for more
    // Out of room, scroll up
    lines.erase(lines.begin());
    --row;
    clear();
    for (int i = 0; i < lines.size(); ++i)
    {
        ssd1306_display_text(device(), i, lines[i].c_str(), lines[i].size(), false);
    }
}

