#include "util.h"

#include <stdio.h>

#include "cJSON.h"

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

cJSON_wrapper::cJSON_wrapper(cJSON*& json)
    : json(json)
{
}

cJSON_wrapper::~cJSON_wrapper()
{
    if (json)
        cJSON_Delete(json);
    json = nullptr;
}

cJSON_Print_wrapper::cJSON_Print_wrapper(char*& json)
    : json(json)
{
}

cJSON_Print_wrapper::~cJSON_Print_wrapper()
{
    if (json)
        free(json);
    json = nullptr;
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:

