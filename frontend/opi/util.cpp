#include "util.h"

#include <ctype.h>

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

    std::string strip_ascii(const std::string& s)
    {
        std::string result;
        for (auto c : s)
            if (!isprint(c))
                result += c;
        return result;
    }
}
