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

bool is_it_thursday()
{
    time_t current = 0;
    time(&current);
    struct tm timeinfo;
    localtime_r(&current, &timeinfo);
    return timeinfo.tm_wday == 4;
}

time_t make_timestamp(char* stamp)
{
    time_t current = 0;
    time(&current);
    make_timestamp(current, stamp);
    return current;
}

void make_timestamp(time_t t, char* stamp)
{
    struct tm timeinfo;
    gmtime_r(&t, &timeinfo);
    strftime(stamp, TIMESTAMP_SIZE, "%Y-%m-%dT%H:%M:%S", &timeinfo);
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

