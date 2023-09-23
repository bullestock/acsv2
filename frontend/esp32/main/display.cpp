#include "defs.h"
#include "display.h"
#include "format.h"

#include <TFT_eSPI.h>

static constexpr const auto small_font = &FreeSans12pt7b;
static constexpr const auto medium_font = &FreeSansBold18pt7b;
static constexpr const auto large_font = &FreeSansBold24pt7b;
static constexpr const int GFXFF = 1;

Display::Display(TFT_eSPI& tft)
    : tft(tft)
{
    tft.init();
    tft.setRotation(1);
    tft.setFreeFont(small_font);
    tft.setTextColor(TFT_CYAN);
    clear();
}

void Display::clear()
{
    tft.fillScreen(TFT_BLACK);
}

void Display::add_progress(const std::string& status)
{
    if (!textheight)
        textheight = tft.fontHeight(GFXFF) + 1;
        
    tft.setTextColor(TFT_WHITE);
    tft.setFreeFont(small_font);
    const auto w = tft.textWidth(status.c_str(), GFXFF);
    if (w > TFT_HEIGHT)
        printf("String '%s' is too wide\n", status.c_str());
    const auto x = TFT_HEIGHT/2 - w/2;
    tft.drawString(status.c_str(), x, row * textheight, GFXFF);
    ++row;
    lines.push_back(status);
    if (row * textheight < TFT_WIDTH)
        return; // still room for more
    lines.erase(lines.begin());
    --row;
    tft.fillScreen(TFT_BLACK);
    for (int i = 0; i < lines.size(); ++i)
    {
        const auto w = tft.textWidth(status.c_str(), GFXFF);
        const auto x = TFT_HEIGHT/2 - w/2;
        tft.drawString(lines[i].c_str(), x, i * textheight, GFXFF);
    }
}

void Display::set_status(const std::string& status, uint16_t colour,
                         bool large)
{
    if (status != last_status)
    {
        tft.fillRect(0, 0, TFT_HEIGHT, TFT_WIDTH/4*3, TFT_BLACK);
        last_status = status;
    }
    tft.setTextColor(colour);
    tft.setFreeFont(large ? large_font : medium_font);
    const auto w = tft.textWidth(status.c_str(), GFXFF);
    if (w > TFT_HEIGHT)
        printf("String '%s' is too wide\n", status.c_str());
    const auto x = TFT_HEIGHT/2 - w/2;
    const auto y = TFT_HEIGHT/4;
    tft.drawString(status.c_str(), x, y, GFXFF);
}

void Display::show_message(const std::string& message, uint16_t colour)
{
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
