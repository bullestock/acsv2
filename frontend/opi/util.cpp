#include "util.h"
#include "logger.h"
#include "slack.h"

#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iomanip>
#include <iostream>

#include <boost/algorithm/string.hpp>

#include <fmt/core.h>

extern Slack_writer slack;

namespace util
{

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

    // Compute number of lines needed
    size_t idx = 0;
    size_t cur_size = 0;
    int nof_lines = 0;
    while (idx < words.size())
    {
        if (cur_size + 1 + words[idx].size() < max_len)
        {
            cur_size += 1;
            cur_size += words[idx].size();
            ++idx;
        }
        else
        {
            ++nof_lines;
            cur_size = 0;
        }
    }
    if (cur_size > 0)
        ++nof_lines;

    std::vector<std::string> lines;
    std::string cur_line;
    const int words_per_line = words.size() / nof_lines;
    idx = 0;
    int words_on_cur_line = 0;
    while (idx < words.size())
    {
        if ((cur_line.size() + 1 + words[idx].size() < max_len) &&
            (words_on_cur_line < words_per_line))
        {
            cur_line += " ";
            cur_line += words[idx];
            ++words_on_cur_line;
            ++idx;
        }
        else
        {
            lines.push_back(cur_line);
            cur_line.clear();
            words_on_cur_line = 0;
        }
    }
    if (!cur_line.empty())
        lines.push_back(cur_line);
    return lines;
}

void unexport_pin(int pin)
{
    Logger::instance().log_verbose(fmt::format("unexport pin {}", pin));
    int fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd == -1)
    {
        Logger::instance().log("Unable to open /sys/class/gpio/unexport");
        return;
    }
    const auto name = fmt::format("{}", pin);
    const auto n = write(fd, name.c_str(), name.size());
    close(fd);
    if (n != name.size())
        Logger::instance().log("Error writing to /sys/class/gpio/unexport");
}

} // end namespace
