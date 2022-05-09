#pragma once

#include "logger.h"

class Buttons
{
public:
    struct Keys
    {
        bool red = false;
        bool white = false;
        bool green = false;
        bool leave = false;
    };

    Buttons(Logger& l);
    
    ~Buttons();

    Keys read(bool log = true);

private:
    void set_pin_input(int pin);
    void unexport_pin(int pin);
    bool read_pin(int pin, bool do_log);

    Logger& logger;
};
