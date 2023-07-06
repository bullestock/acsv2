#include "cardreader.h"
#include "logger.h"
#include "serial.h"
#include "util.h"

constexpr auto SOUND_WARNING_BEEP = "S1000 100\n";
constexpr auto BEEP_INTERVAL = std::chrono::milliseconds(500);
constexpr auto REOPEN_INTERVAL = std::chrono::hours(1);

Card_reader::Card_reader(serialib& p)
    : port(p),
      thread([this](){ thread_body(); })
{
    Logger::instance().log("Card_reader: Starting");
}

Card_reader::~Card_reader()
{
    Logger::instance().log("Card_reader: Stopping");
    stop = true;
    if (thread.joinable())
        thread.join();
}

void Card_reader::set_pattern(Pattern p)
{
    pattern.store(p);
}

void Card_reader::set_sound(Sound s)
{
    sound.store(s);
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
    util::time_point last_sound_change = util::now();
    util::time_point last_reopen = util::now();
    Pattern last_pattern = Pattern::none;
    int n = 0;
    while (!stop)
    {
        ++n;
        if (n > 120)
        {
            Logger::instance().log("Card_reader: I'm alive");
            n = 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        const auto since = util::now() - last_reopen;
        if (since >= REOPEN_INTERVAL)
        {
            Logger::instance().log("Card_reader: Reopening port");
            const auto device = port.currentDevice();
            port.close();
            if (auto res = port.openDevice(device, 115200))
            {
                Logger::instance().log(fmt::format("Failed to reopen {}: {}", device, res));
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                exit(1);
            }
            last_reopen = util::now();
        }
        
        if (!port.write("C\n"))
        {
            Logger::instance().log(fmt::format("Card_reader: Write C failed: {}", errno));
            last_reopen = util::now() - REOPEN_INTERVAL;
            continue;
        }
        std::string line;
        const int nof_bytes = port.readString(line, '\n', 50, 100);
        line = util::strip_np(line);
        //Logger::instance().log(fmt::format("Card_reader: got '{}'", line));
        if ((line.size() == 2+10) && (line.substr(0, 2) == std::string("ID")))
        {
            line = line.substr(2);
            Logger::instance().log(fmt::format("Card_reader: got card ID '{}'", line));
            std::lock_guard<std::mutex> g(mutex);
            card_id = line;
        }
        else if (!line.empty() && line.size() != 2)
        {
            const auto skipped = skip_prologue(port, "Card reader");
            Logger::instance().log(fmt::format("Card_reader: got garbage '{}', skipped {}", line, skipped));
        }
        switch (sound)
        {
        case Sound::warning:
            {
                const auto since = util::now() - last_sound_change;
                if (since >= BEEP_INTERVAL)
                {
                    last_sound_change = util::now();
                    if (!port.write(SOUND_WARNING_BEEP))
                    {
                        Logger::instance().log("Card_reader: Write (beep) failed");
                        continue;
                    }
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
                fatal_error(fmt::format("Unhandled Pattern value: {}",
                                        static_cast<int>(active_pattern)));
                break;
            }
            if (!port.write(cmd))
            {
                Logger::instance().log("Card_reader: Write (pattern) failed");
                continue;
            }
            Logger::instance().log_verbose(fmt::format("Card_reader wrote {}", cmd));
        }
    }
}
