#pragma once

#include "serialib.h"

struct Ports
{
    serialib display;
    serialib lock;
    serialib reader;

    bool complete() const;
};

Ports detect_ports();
