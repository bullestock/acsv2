#pragma once

#include <chrono>

#include <nlohmann/json.hpp>

namespace util
{
    using duration = std::chrono::system_clock::duration;
    using time_point = std::chrono::system_clock::time_point;
    static inline time_point now() { return std::chrono::system_clock::now(); }

    using json = nlohmann::json;
}
