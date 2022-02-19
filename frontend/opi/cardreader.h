#pragma once

#include "serialib.h"

class Card_reader
{
public:
    Card_reader(serialib&);

private:
    serialib& port;
};
