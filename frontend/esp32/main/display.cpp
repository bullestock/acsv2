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
static constexpr const auto status_font = &FreeSans9pt7b;
static constexpr const int GFXFF = 1;
static constexpr const auto MESSAGE_DURATION = std::chrono::seconds(10);

// Top part of screen

static constexpr const int STATUS_HEIGHT = 20;

// Bottom part of screen
static constexpr const int LABEL_HEIGHT = 20;

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
}

void Display::start_uptime_counter()
{
    time(&start_time);
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

void Display::set_status(const std::string& status,
                         uint16_t colour)
{
    if (status != last_status ||
        !last_aux_status.empty())
    {
        clear_status_area();
        last_status = status;
        last_status_colour = colour;
        show_text(status, colour, "", TFT_BLACK);
    }
}

void Display::set_status(const std::string& status,
                         uint16_t colour,
                         const std::string& aux_status,
                         uint16_t aux_colour)
{
    if (status != last_status ||
        aux_status != last_aux_status)
    {
        clear_status_area();
        last_status = status;
        last_status_colour = colour;
        last_aux_status = aux_status;
        last_aux_status_colour = aux_colour;
        show_text(status, colour, aux_status, aux_colour);
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

void Display::show_text(const std::string& status,
                        uint16_t colour,
                        const std::string& aux_status,
                        uint16_t aux_colour)
{
    tft.setTextColor(colour);
    tft.setFreeFont(medium_font);
    const auto h = medium_textheight;
    
    const auto lines = split(status);
    auto nof_lines = lines.size();
    if (!aux_status.empty())
        ++nof_lines;
    auto y = STATUS_HEIGHT + (TFT_WIDTH - STATUS_HEIGHT - LABEL_HEIGHT)/2 - nof_lines/2*h - h/2;
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
    if (!aux_status.empty())
    {
        tft.setTextColor(aux_colour);
        tft.setFreeFont(small_font);
        const auto w = tft.textWidth(aux_status.c_str(), GFXFF);
        const auto x = TFT_HEIGHT/2 - w/2;
        tft.drawString(aux_status.c_str(), x, y, GFXFF);
    }
}

void Display::show_message(const std::string& message, uint16_t colour)
{
    last_message = util::now();
    clear_status_area();
    show_text(message, colour, "", TFT_BLACK);
}

void Display::update()
{
    if (util::is_valid(last_message) &&
        util::now() - last_message >= MESSAGE_DURATION)
    {
        // Clear message, show last status
        last_message = util::invalid_time_point();
        clear_status_area();
        show_text(last_status, last_status_colour, last_aux_status, last_aux_status_colour);
    }
    time_t current = 0;
    time(&current);
    if (current != last_clock)
    {
        last_clock = current;
        ++seconds_since_status_update;
        if (seconds_since_status_update >= 60)
        {
            seconds_since_status_update = 0;
            std::string status;
            switch (status_page)
            {
            case 0:
                {
                    char stamp[Logger::TIMESTAMP_SIZE];
                    Logger::make_timestamp(stamp);
                    status = format("%s - %s", stamp,
                                    Gateway::instance().get_allow_open() ? "AO" : "NO");
                }
                break;

            case 1:
            case 2:
            case 3:
                {
                    time_t now;
                    time(&now);
                    const auto uptime = now - start_time;
                    const int days = uptime/(24*60*60);
                    int minutes = (uptime - days*24*60*60)/60;
                    const int hours = minutes/60;
                    minutes -= hours*60;
                    const auto ip = get_ip_address();
                    char ip_buf[4*(3+1)+1];
                    esp_ip4addr_ntoa(&ip, ip_buf, sizeof(ip_buf));
                    const auto mem = heap_caps_get_free_size(MALLOC_CAP_8BIT);
                    ESP_LOGI(TAG, "Memory %zu", mem);
                    const auto app_desc = esp_app_get_description();
                    status = format("V%s - %s - %dd%02d:%02d - M%d",
                                    app_desc->version, ip_buf,
                                    days, hours, minutes,
                                    static_cast<int>(mem/1024));
                }
                break;
            }
            ++status_page;
            if (status_page > 3)
                status_page = 0;
            // Show status
            tft.fillRect(0, 0, TFT_HEIGHT, STATUS_HEIGHT, TFT_BLACK);
            tft.setTextColor(TFT_OLIVE);
            tft.setFreeFont(status_font);
            tft.drawString(status.c_str(), 0, 0, GFXFF);
            // Show labels
            tft.setFreeFont(small_font);
            tft.fillRect(0, TFT_WIDTH - LABEL_HEIGHT, TFT_HEIGHT, LABEL_HEIGHT, TFT_BLACK);
            tft.setTextColor(TFT_GREEN);
            tft.drawString("Open 15m", 5, TFT_WIDTH - LABEL_HEIGHT, GFXFF);
            tft.setTextColor(TFT_RED);
            tft.drawString("Close", TFT_HEIGHT - 60, TFT_WIDTH - LABEL_HEIGHT, GFXFF);
            tft.setTextColor(TFT_WHITE);
            std::string label;
            if (util::is_it_thursday())
                label = "Thurs";
            else if (Gateway::instance().get_allow_open())
                label = "OPEN";
            if (!label.empty())
            {
                const auto w = tft.textWidth(label.c_str(), GFXFF);
                tft.drawString(label.c_str(), (TFT_HEIGHT - w)/2, TFT_WIDTH - LABEL_HEIGHT, GFXFF);
            }
        }
    }
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
