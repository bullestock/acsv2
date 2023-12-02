#include "cardreader.h"

#include "cardcache.h"
#include "defs.h"
#include "format.h"
#include "logger.h"
#include "rs485.h"
#include "util.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

constexpr auto SOUND_WARNING_BEEP = "S1000 100\n";
constexpr auto BEEP_INTERVAL = std::chrono::milliseconds(500);
constexpr auto REOPEN_INTERVAL = std::chrono::hours(1);

Card_reader& Card_reader::instance()
{
    static Card_reader the_instance;
    return the_instance;
}

void Card_reader::set_pattern(Pattern p)
{
    pattern.store(p);
}

void Card_reader::set_sound(Sound s)
{
    sound.store(s);
}

Card_cache::Card_id Card_reader::get_and_clear_card_id()
{
    std::lock_guard<std::mutex> g(mutex);
    Card_id id = 0;
    std::swap(id, card_id);
    return id;
}

void Card_reader::thread_body()
{
    util::time_point last_sound_change = util::now();
    Pattern last_pattern = Pattern::none;
    int n = 0;
    while (1)
    {
        ++n;
        if (n > 120)
        {
            Logger::instance().log("Card_reader: I'm alive");
            n = 0;
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);

        write_rs485("C\n", 2);
        vTaskDelay(5 / portTICK_PERIOD_MS);

        char buf[40];
        const int nof_bytes = read_rs485(buf, sizeof(buf) - 1);
        buf[nof_bytes] = 0;
        std::string line(buf);
        line = util::strip_np(line);
#ifdef DETAILED_DEBUG
        ESP_LOGI(TAG, "Card_reader: got '%s'", line.c_str());
#endif
        if ((line.size() == 2+10) && (line.substr(0, 2) == std::string("ID")))
        {
            line = line.substr(2);
            Logger::instance().log(format("Card_reader: got card ID '%s'", line.c_str()));
            const auto new_card_id = Card_cache::get_id_from_string(line);
            if (new_card_id)
            {
                std::lock_guard<std::mutex> g(mutex);
                card_id = new_card_id;
            }
        }
        switch (sound)
        {
        case Sound::warning:
            {
                const auto since = util::now() - last_sound_change;
                if (since >= BEEP_INTERVAL)
                {
                    last_sound_change = util::now();
                    write_rs485(SOUND_WARNING_BEEP, strlen(SOUND_WARNING_BEEP));
                }
            }
            break;

        case Sound::warn_closing:
            //!!
            break;

        case Sound::none:
            break;
        }
        const auto active_pattern = pattern.exchange(Pattern::none);
        if (active_pattern != last_pattern)
        {
            last_pattern = active_pattern;
            std::string cmd;
            switch (active_pattern)
            {
            case Pattern::ready:
                cmd = "P200R10SGN\n";
                break;
            case Pattern::enter:
                cmd = "P250R8SGN\n";
                break;
            case Pattern::open:
                cmd = "P200R0SG\n";
                break;
            case Pattern::warn_closing:
                cmd = "P5R0SGX10NX100R\n";
                break;
            case Pattern::error:
                cmd = "P5R10SGX10NX100RX100N\n";
                break;
            case Pattern::no_entry:
                cmd = "P100R30SRN\n";
                break;
            case Pattern::wait:
                cmd = "P20R0SGNN\n";
                break;
            case Pattern::none:
                break;
            default:
                fatal_error(format("Unhandled Pattern value: {}", static_cast<int>(active_pattern)).c_str());
                break;
            }
            if (!cmd.empty())
            {
                write_rs485(cmd.c_str(), cmd.size());
#ifdef DETAILED_DEBUG
                Logger::instance().log_verbose(format("Card_reader wrote %s", cmd.c_str()));
#endif
            }
        }
    }
}

void card_reader_task(void*)
{
    Card_reader::instance().thread_body();
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
