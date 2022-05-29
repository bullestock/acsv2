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

    void fatal_error(const std::string& msg);

    void set_handlers(std::function<void(const std::string&)> logfun,
                      std::function<void(const std::string&)> logvfun,
                      std::function<void(const std::string&)> fefun);

private:
    static void discard(const std::string&)
    {
    }
    
    std::function<void(const std::string&)> log_fn = &Logger::discard;
    std::function<void(const std::string&)> log_verbose_fn = &Logger::discard;
    std::function<void(const std::string&)> fatal_error_fn = &Logger::discard;
};
