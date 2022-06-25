#pragma once

#include <functional>
#include <string>

/// Logger singleton
class Logger
{
public:
    static Logger& instance();

    void log(const std::string&);

    void log_verbose(const std::string&);

    void set_handlers(std::function<void(const std::string&)> logfun,
                      std::function<void(const std::string&)> logvfun);

private:
    static void discard(const std::string&)
    {
    }
    
    std::function<void(const std::string&)> log_fn = &Logger::discard;
    std::function<void(const std::string&)> log_verbose_fn = &Logger::discard;
};

// Needed in main(), so we just use a global function
extern void fatal_error(const std::string& msg);
