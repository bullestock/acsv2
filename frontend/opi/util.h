#pragma once

#include <chrono>

#include <nlohmann/json.hpp>

static auto constexpr const VERSION = "0.1";

namespace util
{
    using duration = std::chrono::system_clock::duration;
    using time_point = std::chrono::system_clock::time_point;
    static inline time_point now() { return std::chrono::system_clock::now(); }

    using json = nlohmann::json;

    /// Strip trailing/leading whitespace
    std::string strip(const std::string&);

    /// Strip non-printable chars
    std::string strip_ascii(const std::string&);
}
