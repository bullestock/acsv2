#pragma once

#include "serialib.h"

struct Ports
{
    serialib display;
    serialib lock;
    serialib reader;
};

Ports detect_ports();
