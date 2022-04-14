#include "cardcache.h"
#include "cardreader.h"
#include "controller.h"
#include "display.h"
#include "lock.h"
#include "serial.h"
#include "slack.h"
#include "util.h"

#include <fmt/core.h>

#include <iomanip>
#include <iostream>

#include <boost/program_options.hpp>

Slack_writer slack;

int main(int argc, char* argv[])
{
    using namespace boost::program_options;

    bool option_verbose = false;
    bool use_slack = false;
    bool in_prod = false;
    options_description options_desc{ "Options" };
    options_desc.add_options()
       ("help,h", "Help")
       ("production", bool_switch(&in_prod), "Run in production mode")
       ("slack,s", bool_switch(&use_slack), "Use Slack")
       ("verbose,v", bool_switch(&option_verbose), "Verbose output");

    variables_map vm;
    store(command_line_parser(argc, argv).options(options_desc).run(), vm);
    notify(vm);
    if (vm.count("help"))
    {
        std::cout << options_desc;
        exit(0);
    }

#if 0
    Card_cache cc;

    Card_cache::Card_id fl15 = 0x13006042CF;
    std::cout << "fl15: " << cc.has_access(fl15) << std::endl;
    std::cout << "fl15: " << cc.has_access(fl15) << std::endl;
#endif

    slack.set_params(use_slack, !in_prod);

    auto ports = detect_ports(option_verbose);
    if (!ports.display.is_open())
        util::fatal_error("No display found");
    if (!ports.reader.is_open())
        util::fatal_error("No card reader found");
    if (!ports.lock.is_open())
        util::fatal_error("No lock found");

    std::cout << "Found all ports\n";

    slack.send_message(fmt::format("ACS frontend {}", VERSION));
    
    Display display(ports.display);
    display.set_status("HAL9K ACS");

    Card_reader reader(ports.reader);

    Lock lock(ports.lock);

    Controller c(slack, display, reader, lock);
    c.run();
}
