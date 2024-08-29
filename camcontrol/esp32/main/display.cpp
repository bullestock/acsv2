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
    std::string txt = std::string(" ") + status;
    ssd1306_display_text(device(), row, txt.c_str(), txt.size(), false);
    ++row;
    lines.push_back(status);
    if (row < 7)
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

void Display::set_status(bool cam_on, bool estop_on)
{
    std::string estop_status = "   e-stop: ";
    estop_status += estop_on ? "ON" : "OFF";
    set_status(cam_on ? "  On" : " Off",
               estop_status);
}

void Display::set_status(const std::string& main_status,
                         const std::string& aux_status)
{
    clear();
    ssd1306_display_text(device(), 0, "    CAMERAS", 11, false);
    ssd1306_display_text_x3(device(), 3, main_status.c_str(), main_status.size(), false);
    ssd1306_display_text(device(), 7, aux_status.c_str(), aux_status.size(), false);
}
