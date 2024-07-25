#include "connect.h"
#include "defs.h"
#include "display.h"
#include "format.h"
#include "gateway.h"
#include "logger.h"

#include <TFT_eSPI.h>

#include "esp_app_desc.h"
#include <esp_heap_caps.h>

static constexpr const auto small_font = &FreeSans12pt7b;
static constexpr const auto medium_font = &FreeSansBold18pt7b;
static constexpr const auto large_font = &FreeSansBold24pt7b;
static constexpr const int GFXFF = 1;
static constexpr const auto MESSAGE_DURATION = std::chrono::seconds(10);

// Top part of screen

static constexpr const int STATUS_HEIGHT = 20;

#define DEBUG(x)
//#define DEBUG(x) printf x

Display::Display(TFT_eSPI& tft)
    : tft(tft)
{
    tft.init();
    tft.setRotation(1);
    tft.setTextColor(TFT_CYAN);
    clear();

    tft.setFreeFont(small_font);
    small_textheight = tft.fontHeight(GFXFF) + 1;
    tft.setFreeFont(medium_font);
    medium_textheight = tft.fontHeight(GFXFF) + 1;
    tft.setFreeFont(large_font);
    large_textheight = tft.fontHeight(GFXFF) + 1;
}

void Display::clear()
{
    tft.fillScreen(TFT_BLACK);
}

void Display::add_progress(const std::string& status)
{
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
}

void Display::set_status(const std::string& status, uint16_t colour,
                         bool large)
{
    if (status != last_status)
    {
        clear_status_area();
        last_status = status;
        last_status_colour = colour;
        last_status_large = large;
        show_text(status, colour, large);
    }
}

void Display::clear_status_area()
{
    tft.fillRect(0, STATUS_HEIGHT, TFT_HEIGHT, TFT_WIDTH - STATUS_HEIGHT - LABEL_HEIGHT, TFT_BLACK);
}

static std::vector<std::string> split(const std::string& s)
{
    std::vector<std::string> v;
    std::string::size_type curpos = 0;
    while (curpos < s.size())
    {
        const auto end = s.find('\n', curpos);
        if (end == std::string::npos)
        {
            // Last line
            v.push_back(s.substr(curpos));
            return v;
        }
        const auto line = s.substr(curpos, end - curpos);
        v.push_back(line);
        curpos = end + 1;
    }
    return v;
}

void Display::show_text(const std::string& status, uint16_t colour,
                        bool large)
{
    tft.setTextColor(colour);
    tft.setFreeFont(large ? large_font : medium_font);
    const auto h = large ? large_textheight : medium_textheight;
    
    const auto lines = split(status);
    auto y = STATUS_HEIGHT + (TFT_WIDTH - STATUS_HEIGHT - LABEL_HEIGHT)/2 - lines.size()/2*h - h/2;
    for (const auto& line : lines)
    {
        const auto w = tft.textWidth(line.c_str(), GFXFF);
        if (w > TFT_HEIGHT)
            printf("String '%s' is too wide\n", line.c_str());
        const auto x = TFT_HEIGHT/2 - w/2;
        tft.drawString(line.c_str(), x, y, GFXFF);
        printf("At %d, %d: %s\n", x, y, line.c_str());
        y += h;
    }
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
