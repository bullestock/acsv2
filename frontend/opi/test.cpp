#include "test.h"

#include "buttons.h"
#include "cardcache.h"
#include "cardreader.h"
#include "display.h"
#include "logger.h"
#include "lock.h"
#include "slack.h"

class Dummy_logger : public Logger
{
    void log(const std::string& s) override {}
    void log_verbose(const std::string&) override {}
    void fatal_error(const std::string& msg) override {}
};

bool run_test(const std::string& arg)
{
    if (arg == "cardcache")
    {
        Card_cache cc;

        Card_cache::Card_id fl15 = 0x13006042CF;
        std::cout << "fl15: " << cc.has_access(fl15) << std::endl;
        std::cout << "fl15: " << cc.has_access(fl15) << std::endl;
        return true;
    }
    if (arg == "buttons")
    {
        Dummy_logger dl;
        Buttons b(dl);
        while (1)
        {
            const auto keys = b.read();
            std::cout << fmt::format("R {:d} W {:d} G {:d} L {:d}\n", keys.red, keys.white, keys.green, keys.leave);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        return true;
    }
    return false;
}

bool run_test(const std::string& arg,
              Slack_writer& slack,
              Display& display,
              Card_reader& reader,
              Lock& lock)
{
    if (arg.empty())
        return false;
    if (arg == "display")
    {
        display.clear();
        display.set_status("Please close the door and raise the handle", Display::Color::red);
        return true;
    }
    std::cerr << "Unrecognized argument to --test\n";
    return true;
}
