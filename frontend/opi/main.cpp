#include "cardcache.h"
#include "cardreader.h"
#include "controller.h"
#include "display.h"
#include "lock.h"
#include "logger.h"
#include "serial.h"
#include "slack.h"
#include "test.h"
#include "util.h"

#include <fmt/core.h>

#include <iomanip>
#include <iostream>

#include <boost/program_options.hpp>

Slack_writer slack;
Display display;

void fatal_error(const std::string& msg)
{
    display.set_status(msg, Display::Color::red);
    const auto s = fmt::format("Fatal error: {}", msg);
    Logger::instance().log(s);
    slack.set_status(fmt::format(":stop: {}", s));
    if (Controller::exists())
        display.show_info(9, "Press a key to restart", Display::Color::white);
    const auto start = util::now();
    const auto dur = std::chrono::minutes(5);
    while (1)
    {
        const auto elapsed = util::now() - start;
        if (elapsed > dur)
            break;
        display.show_info(8,
                          fmt::format("Restart in {} secs", std::chrono::duration_cast<std::chrono::seconds>(elapsed - dur).count()),
                          Display::Color::white);
        if (Controller::exists())
        {
            const auto keys = Controller::instance().read_keys(false);
            if (keys.green || keys.white || keys.red)
            {
                display.clear();
                display.set_status("RESTARTING", Display::Color::orange);
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
    exit(1);
}

int main(int argc, char* argv[])
{
    using namespace boost::program_options;

    bool option_verbose = false;
    bool use_slack = false;
    bool in_prod = false;
    bool log_to_gw = false;
    std::string test_arg;
    options_description options_desc{ "Options" };
    options_desc.add_options()
       ("help,h", "Help")
       ("log-to-gateway", bool_switch(&log_to_gw), "Log to gateway (default is to stdout)")
       ("production", bool_switch(&in_prod), "Run in production mode")
       ("slack,s", bool_switch(&use_slack), "Use Slack")
       ("test", value(&test_arg), "Run test")
       ("verbose,v", bool_switch(&option_verbose), "Verbose output");

    variables_map vm;
    store(command_line_parser(argc, argv).options(options_desc).run(), vm);
    notify(vm);
    if (vm.count("help"))
    {
        std::cout << options_desc;
        exit(0);
    }

    if (run_test(test_arg))
        return 0;

    slack.set_params(use_slack, !in_prod);
    Logger::instance().set_verbose(option_verbose);
    Logger::instance().set_log_to_gateway(log_to_gw);

    slack.send_message(fmt::format(":waiting: ACS frontend {}", VERSION));
    
    auto ports = detect_ports();
    if (!ports.display.is_open())
        fatal_error("No display found");

    display.set_port(ports.display);
    display.set_status("HAL9K ACS");

    if (!ports.reader.is_open())
        fatal_error("No card reader found");
    if (!ports.lock.is_open())
        fatal_error("No lock found");

    Logger::instance().log("Found all ports");

    Card_reader reader(ports.reader);

    Lock lock(ports.lock);

    if (run_test(test_arg, slack, display, reader, lock))
    {
        std::cout << "Exiting\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));
        return 0;
    }

    Controller c(slack, display, reader, lock);
    c.run();
}
