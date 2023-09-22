#include "util.h"

#include <stdio.h>

void fatal_error(const char* msg)
{
    printf("FATAL: %s\n", msg);
    // TODO
    
}

namespace util
{

std::string strip_np(const std::string& s)
{
    std::string result;
    for (auto c : s)
        if (isprint(c))
            result += c;
    return result;
}

} // end namespace

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:

