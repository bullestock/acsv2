#include "test.h"

#include "buttons.h"
#include "cardcache.h"
#include "cardreader.h"
#include "display.h"
#include "foreninglet.h"
#include "logger.h"
#include "lock.h"
#include "slack.h"

bool run_test(const std::string& arg)
{
    if (arg == "cardcache")
    {
        Card_cache cc;

        Card_cache::Card_id fl15 = 0x13006042CF;
        std::cout << "fl15: " << static_cast<int>(cc.has_access(fl15).access) << std::endl;
        std::cout << "fl15: " << static_cast<int>(cc.has_access(fl15).access) << std::endl;
        return true;
    }
    if (arg == "buttons")
    {
        Buttons b;
        while (1)
        {
            const auto keys = b.read();
            std::cout << fmt::format("R {:d} W {:d} G {:d} L {:d}\n", keys.red, keys.white, keys.green, keys.leave);
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        return true;
    }
    if (arg == "foreninglet")
    {
        ForeningLet::instance().update_last_access(172047, util::now());
        std::this_thread::sleep_for(std::chrono::seconds(3));
        ForeningLet::instance().destroy();
        return true;
    }
    return false;
}

bool run_test(const std::string& arg,
              Slack_writer& slack,
              Display& display,
              Card_reader& reader,
              Lock_base& lock)
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
