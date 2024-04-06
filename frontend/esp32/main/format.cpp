#include "format.h"

#include <stdarg.h>
#include <stdio.h>

std::string format(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    
    return std::string(buf);
}
