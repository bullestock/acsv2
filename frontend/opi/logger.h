#pragma once

#include <functional>
#include <string>

/// Logger singleton
class Logger
{
public:
    static Logger& instance();

    void set_verbose(bool on)
    {
        verbose = on;
    }

    void set_log_to_gateway(bool on)
    {
        log_to_gateway = on;
    }

    void log(const std::string&);

    void log_verbose(const std::string&);

private:
    Logger();
    
    std::string token;
    bool verbose = false;
    bool log_to_gateway = false;
};

// Needed in main(), so we just use a global function
extern void fatal_error(const std::string& msg);
