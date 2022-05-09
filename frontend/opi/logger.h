#pragma once

#include <string>

/// Logger ABC
class Logger
{
public:
    virtual void log(const std::string&) = 0;

    virtual void log_verbose(const std::string&) = 0;

    virtual void fatal_error(const std::string& msg) = 0;
};

