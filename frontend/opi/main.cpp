#include "cardcache.h"
#include "serial.h"
#include "slack.h"

#include <fmt/core.h>

#include <iomanip>
#include <iostream>

void fatal_error(Slack_writer& slack, const std::string& msg)
{
    std::cout << "Fatal error: " << msg << std::endl;
    slack.send_message(fmt::format(":stop: {}", msg));
    exit(1);
}

int main()
{
#if 0
    Card_cache cc;

    Card_cache::Card_id fl15 = 0x13006042CF;
    std::cout << "fl15: " << cc.has_access(fl15) << std::endl;
    std::cout << "fl15: " << cc.has_access(fl15) << std::endl;
#endif

    Slack_writer slack(true, true);

    auto ports = detect_ports();
    if (!ports.display.is_open())
        fatal_error(slack, "No display found");
    if (!ports.reader.is_open())
        fatal_error(slack, "No card reader found");
    if (!ports.lock.is_open())
        fatal_error(slack, "No lock found");

}
