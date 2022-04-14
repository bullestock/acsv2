#pragma once

#include <chrono>
#include <string>

#include <fmt/core.h>

#include <nlohmann/json.hpp>

static auto constexpr const VERSION = "0.1";

class Slack_writer;

namespace util
{
    void fatal_error(const std::string& msg);

    template <typename... Args>
    void fatal_error(const std::string& fmt_string, Args... args)
    {
        return fatal_error(fmt::format(fmt_string, std::forward<Args>(args)...));
    }
    
    using duration = std::chrono::system_clock::duration;
    using time_point = std::chrono::system_clock::time_point;

    static inline time_point now()
    {
        return std::chrono::system_clock::now();
    }

    constexpr time_point invalid_time_point()
    {
        return time_point();
    }

    constexpr duration invalid_duration()
    {
        return duration::min();
    }

    constexpr bool is_valid(const time_point& ts)
    {
        return ts != invalid_time_point();
    }

    constexpr bool is_valid(const duration& dur)
    {
        return dur != invalid_duration();
    }

    using json = nlohmann::json;

    /// Strip trailing/leading whitespace
    std::string strip(const std::string&);

    /// Strip non-printable chars
    std::string strip_ascii(const std::string&);
}
