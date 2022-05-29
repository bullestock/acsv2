#pragma once

#include <chrono>
#include <string>

#include <fmt/core.h>

#include <nlohmann/json.hpp>

static auto constexpr const VERSION = "0.1";

class Slack_writer;

namespace util
{

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
std::string strip_np(const std::string&);

std::vector<std::string> split(const std::string& s, const std::string& sep);

template<typename T>
bool from_string(const std::string& s, T& val)
{
   T temp;
   std::stringstream ss(s);
   if (!(ss >> temp))
       return false;
   val = temp;
   return true;
}

std::vector<std::string> wrap(const std::string& s, size_t max_len);

} // end namespace
