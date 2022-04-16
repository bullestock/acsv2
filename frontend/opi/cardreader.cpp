#include "cardreader.h"
#include "controller.h"
#include "util.h"

constexpr auto SOUND_WARNING_BEEP = "S1000 100\n";
constexpr auto BEEP_INTERVAL = std::chrono::milliseconds(500);

Card_reader::Card_reader(serialib& p)
    : port(p),
      thread([this](){ thread_body(); })
{
}

void Card_reader::set_pattern(Pattern p)
{
    pattern.store(p);
}

void Card_reader::start_beep()
{
    beep_on = true;
}
    
void Card_reader::stop_beep()
{
    beep_on = false;
}

std::string Card_reader::get_and_clear_card_id()
{
    std::lock_guard<std::mutex> g(mutex);
    std::string id;
    std::swap(id, card_id);
    return id;
}

void Card_reader::thread_body()
{
    util::time_point last_beep = util::now();
    while (1)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (!port.write("C\n"))
        {
            std::cout << "Card_reader: Write C failed\n";
            continue;
        }
        std::string line;
        const int nof_bytes = port.readString(line, '\n', 50, 100);
        if (line.size() > 2+10)
        {
            line = util::strip(line).substr(2);
            Controller::instance().log(fmt::format("Card_reader: got card ID '{}'", line));
            std::lock_guard<std::mutex> g(mutex);
            card_id = line;
        }
        if (beep_on)
        {
            const auto since = util::now() - last_beep;
            if (since >= BEEP_INTERVAL)
            {
                last_beep = util::now();
                if (!port.write(SOUND_WARNING_BEEP))
                {
                    std::cout << "Card_reader: Write (beep) failed\n";
                    continue;
                }
            }
        }
        const auto active_pattern = pattern.exchange(Pattern::none);
        if (active_pattern != Pattern::none)
        {
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
            default:
                util::fatal_error(fmt::format("Unhandled Pattern value: {}",
                                              static_cast<int>(active_pattern)));
                break;
            }
            if (!port.write(cmd))
            {
                std::cout << "Card_reader: Write (pattern) failed\n";
                continue;
            }
            Controller::instance().log(fmt::format("Card_reader wrote {}", cmd));
        }
    }
}
