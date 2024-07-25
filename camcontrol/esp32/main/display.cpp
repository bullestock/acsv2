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
    /*
    tft.setTextColor(TFT_WHITE);
    tft.setFreeFont(small_font);
    const auto w = tft.textWidth(status.c_str(), GFXFF);
    if (w > TFT_HEIGHT)
        printf("String '%s' is too wide\n", status.c_str());
    const auto x = TFT_HEIGHT/2 - w/2;
    tft.drawString(status.c_str(), x, row * small_textheight, GFXFF);
    ++row;
    lines.push_back(status);
    if (row * small_textheight < TFT_WIDTH)
        return; // still room for more
    // Out of room, scroll up
    lines.erase(lines.begin());
    --row;
    tft.fillScreen(TFT_BLACK);
    DEBUG(("Scrolling\n"));
    for (int i = 0; i < lines.size(); ++i)
    {
        const auto w = tft.textWidth(lines[i].c_str(), GFXFF);
        const auto x = TFT_HEIGHT/2 - w/2;
        DEBUG(("At %d, %d: %s\n", x, i * small_textheight, lines[i].c_str()));
        tft.drawString(lines[i].c_str(), x, i * small_textheight, GFXFF);
    }
    */
}

