#pragma once

#include "serialib.h"

class Card_reader
{
public:
    enum class Pattern {
        ready,
        enter,
        open,
        warn_closing,
    };
    
    Card_reader(serialib&);

    void set_pattern(Pattern);

    void start_beep();
    
    void stop_beep();
    
private:
    serialib& port;
};
