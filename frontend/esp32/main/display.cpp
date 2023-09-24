#include "defs.h"
#include "display.h"
#include "format.h"

#include <TFT_eSPI.h>

#include <esp_heap_caps.h>

static constexpr const auto small_font = &FreeSans12pt7b;
static constexpr const auto medium_font = &FreeSansBold18pt7b;
static constexpr const auto large_font = &FreeSansBold24pt7b;
static constexpr const auto time_font = &FreeMonoBold12pt7b;
static constexpr const int GFXFF = 1;
static constexpr const auto MESSAGE_DURATION = std::chrono::seconds(10);
static constexpr const int TIME_HEIGHT = 20;

Display::Display(TFT_eSPI& tft)
    : tft(tft)
{
    tft.setFreeFont(small_font);
    small_textheight = tft.fontHeight(GFXFF) + 1;
    tft.setFreeFont(medium_font);
    medium_textheight = tft.fontHeight(GFXFF) + 1;
    tft.setFreeFont(large_font);
    large_textheight = tft.fontHeight(GFXFF) + 1;
        
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
    lines.erase(lines.begin());
    --row;
    tft.fillScreen(TFT_BLACK);
    for (int i = 0; i < lines.size(); ++i)
    {
        const auto w = tft.textWidth(status.c_str(), GFXFF);
        const auto x = TFT_HEIGHT/2 - w/2;
        tft.drawString(lines[i].c_str(), x, i * small_textheight, GFXFF);
    }
}

void Display::set_status(const std::string& status, uint16_t colour,
                         bool large)
{
    if (status != last_status)
    {
        printf("set_status: new %s\n", status.c_str());
        clear_status_area();
        last_status = status;
        last_status_colour = colour;
        last_status_large = large;
        show_text(status, colour, large);
    }
}

void Display::clear_status_area()
{
    tft.fillRect(0, 0, TFT_HEIGHT, TFT_WIDTH - TIME_HEIGHT, TFT_BLACK);
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
    auto y = (TFT_WIDTH - TIME_HEIGHT)/2 - lines.size()/2*h - h/2;
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

void Display::show_message(const std::string& message, uint16_t colour)
{
    last_message = util::now();
    clear_status_area();
    show_text(message, colour, false);
}

void Display::update()
{
    if (util::is_valid(last_message) &&
        util::now() - last_message >= MESSAGE_DURATION)
    {
        last_message = util::invalid_time_point();
        clear_status_area();
        show_text(last_status, last_status_colour, last_status_large);
    }
    time_t current = 0;
    time(&current);
    if (current != last_clock)
    {
        char stamp[26];
        struct tm timeinfo;
        gmtime_r(&current, &timeinfo);
        strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
        last_clock = current;
        tft.fillRect(0, TFT_WIDTH - TIME_HEIGHT, TFT_HEIGHT, TIME_HEIGHT, TFT_BLACK);
        tft.setTextColor(TFT_CYAN);
        tft.setFreeFont(time_font);
        if (clock_x == 0)
        {
            const auto w = tft.textWidth(stamp, GFXFF);
            clock_x = TFT_HEIGHT/2 - w/2;
        }
        tft.drawString(stamp, clock_x, TFT_WIDTH - TIME_HEIGHT + 0, GFXFF);
        ++uptime;
        if (!(uptime % 64))
        {
            ESP_LOGI(TAG, "Uptime %" PRIu64 " memory %zu",
                     uptime,
                     heap_caps_get_free_size(MALLOC_CAP_8BIT));
        }
    }
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
