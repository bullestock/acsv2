#include "cardreader.h"

#include "cardcache.h"
#include "controller.h"
#include "defs.h"
#include "format.h"
#include "mqtt.h"
#include "rs485.h"
#include "util.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static constexpr const char* TAG = "card";

constexpr auto SOUND_WARNING_BEEP = "S1000 100\n";
constexpr auto BEEP_INTERVAL = std::chrono::milliseconds(500);
constexpr auto REOPEN_INTERVAL = std::chrono::hours(1);

Card_reader& Card_reader::instance()
{
    static Card_reader the_instance;
    return the_instance;
}

void Card_reader::set_pattern(Pattern p, int duration)
{
    std::lock_guard<std::mutex> g(pattern_mutex);
    if (pattern_duration < 0)
    {
        pattern = p;
        pattern_duration = duration;
    }
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
    auto last_tick_count = xTaskGetTickCount();
    while (1)
    {
        vTaskDelay(500 / portTICK_PERIOD_MS);

#ifdef DETAILED_DEBUG
        ESP_LOGI(TAG, "Sending 'C'");
#endif
        write_rs485("C\n", 2);
        vTaskDelay(30 / portTICK_PERIOD_MS); // delay needed to ensure we receive the answer

        char buf[40];
        const int nof_bytes = read_rs485(buf, sizeof(buf) - 1);
        buf[nof_bytes] = 0;
        std::string line(buf);
        line = util::strip_np(line);
#ifdef DETAILED_DEBUG
        if (!line.empty())
            ESP_LOGI(TAG, "Got '%s'", line.c_str());
#endif
        if (nof_bytes && Controller::exists())
        {
            Controller::instance().card_reader_heartbeat();
        }
        if ((line.size() == 2+10) && (line.substr(0, 2) == std::string("ID")))
        {
            line = line.substr(2);
            Mqtt::instance().log(format("Card_reader: got card ID '%s'", line.c_str()));
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
        Pattern new_pattern = Pattern::undefined;
        int new_pattern_duration = -1;
        {
            std::lock_guard<std::mutex> g(pattern_mutex);
            new_pattern = pattern;
            new_pattern_duration = pattern_duration;
        }
        if (pattern_duration_left < 0)
        {
            active_pattern = new_pattern;
            pattern_duration_left = new_pattern_duration;
        }
        if (pattern_duration_left >= 0)
        {
            auto tick_count = xTaskGetTickCount();
            if (tick_count - last_tick_count >= 1000/portTICK_PERIOD_MS)
            {
                last_tick_count = tick_count;
                --pattern_duration_left;
            }
            if (pattern_duration_left < 0)
            {
                std::lock_guard<std::mutex> g(pattern_mutex);
                pattern_duration = -1;
            }
        }
        if (active_pattern != Pattern::undefined)
        {
            // Delay is needed to avoid interference from previous read - 500 is enough
            vTaskDelay(50 / portTICK_PERIOD_MS);
            static char cmd[40];
            switch (active_pattern)
            {
            case Pattern::ready:
                strcpy(cmd, "P200R10SGNX5\n");
                break;
            case Pattern::enter:
                strcpy(cmd, "P250R8SGX5N\n");
                break;
            case Pattern::open:
                strcpy(cmd, "P200R0SG\n");
                break;
            case Pattern::warn_closing:
                strcpy(cmd, "P5R0SGX10NX100R\n");
                break;
            case Pattern::error:
                strcpy(cmd, "P5R10SGX10NX100RX100N\n");
                break;
            case Pattern::no_entry:
                strcpy(cmd, "P100R30SRN\n");
                break;
            case Pattern::wait:
                strcpy(cmd, "P20R0SGNN\n");
                break;
            case Pattern::none:
                strcpy(cmd, "");
                break;
            default:
                fatal_error(format("Unhandled Pattern value: {}", static_cast<int>(active_pattern)).c_str());
                break;
            }
            if (strlen(cmd))
            {
                write_rs485(cmd, strlen(cmd));
                vTaskDelay(50 / portTICK_PERIOD_MS);
#ifdef DETAILED_DEBUG
                ESP_LOGI(TAG, "Card_reader wrote %s", cmd);
#endif
                const int nof_bytes = read_rs485(buf, sizeof(buf) - 1);
                buf[nof_bytes] = 0;
                std::string line(buf);
                line = util::strip_np(line);
#ifdef DETAILED_DEBUG
                if (!line.empty())
                    ESP_LOGI(TAG, "Got '%s'", line.c_str());
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
