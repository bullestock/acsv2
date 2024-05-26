#pragma once

#include <chrono>

void fatal_error(const char* msg);

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

std::string strip_np(const std::string& s);

bool is_it_thursday();

} // end namespace

struct cJSON;

class cJSON_wrapper
{
public:
    cJSON_wrapper(cJSON*& json);

    ~cJSON_wrapper();

private:
    cJSON*& json;
};

class cJSON_Print_wrapper
{
public:
    cJSON_Print_wrapper(char*& json);

    ~cJSON_Print_wrapper();

private:
    char*& json;
};
