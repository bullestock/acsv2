#include "util.h"
#include "slack.h"

#include <ctype.h>

#include <iomanip>
#include <iostream>

#include <fmt/core.h>

namespace util
{
void fatal_error(Slack_writer& slack, const std::string& msg)
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
}
