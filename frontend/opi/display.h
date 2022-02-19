#pragma once

#include "serialib.h"

class Display
{
public:
    Display(serialib&);

private:
    serialib port;
};

