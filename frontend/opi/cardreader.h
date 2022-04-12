#pragma once

#include "serialib.h"

class Card_reader
{
public:
    enum class Pattern {
        ready,
        enter,
    };
    
    Card_reader(serialib&);

    void set_pattern(Pattern);
    
private:
    serialib& port;
};
