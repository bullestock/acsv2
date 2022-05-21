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

std::string strip_np(const std::string& s)
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

std::vector<std::string> wrap(const std::string& s, size_t max_len)
{
    const auto words = split(s, " ");
    std::vector<std::string> lines;
    std::string cur_line;
    size_t idx = 0;
    while (idx < words.size())
    {
        if (cur_line.size() + 1 + words[idx].size() < max_len)
        {
            cur_line += " ";
            cur_line += words[idx];
            ++idx;
        }
        else
        {
            lines.push_back(cur_line);
            cur_line.clear();
        }
    }
    if (!cur_line.empty())
        lines.push_back(cur_line);
    return lines;
}

} // end namespace
