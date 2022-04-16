#include "util.h"
#include "slack.h"

#include <ctype.h>

#include <iomanip>
#include <iostream>

#include <boost/algorithm/string.hpp>

#include <fmt/core.h>

extern Slack_writer slack;

namespace util
{
void fatal_error(const std::string& msg)
{
    std::cout << "Fatal error: " << msg << std::endl;
    slack.send_message(fmt::format(":stop: {}", msg));
    exit(1);
}

std::string strip(const std::string& s)
{
    size_t i = 0;
    while (i < s.size() && isspace(s[i]))
        ++i;
    if (i > 0)
        --i;
    auto result = s.substr(i);
    while (!result.empty() && isspace(result.back()))
    {
        result.erase(result.size() - 1);
    }
    return result;
}

std::string strip_ascii(const std::string& s)
{
    std::string result;
    for (auto c : s)
        if (isprint(c))
            result += c;
    return result;
}

std::vector<std::string> split(const std::string& s, const std::string& sep)
{
    std::vector<std::string> v;
    if (s.empty())
        return v;
    boost::split(v, s, boost::is_any_of(sep));
    return v;
}

std::pair<std::string, std::string> wrap(const std::string& s)
{
    auto first_space_pos = s.find_last_of(" ", s.size()/2);
    auto last_space_pos = s.find(" ", s.size()/2);
    std::string::size_type break_pos = 0;
    if (first_space_pos == std::string::npos && last_space_pos != std::string::npos)
        break_pos = last_space_pos;
    else if (first_space_pos != std::string::npos && last_space_pos == std::string::npos)
        break_pos = first_space_pos;
    else
        break_pos = s.size()/2 - first_space_pos < last_space_pos - s.size()/2 ? first_space_pos : last_space_pos;
    return std::make_pair(s.substr(0, break_pos), s.substr(break_pos+1));
}

} // end namespace
