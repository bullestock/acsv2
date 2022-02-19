#pragma once

#include "serialib.h"

class Lock
{
public:
    enum class State {
        Open,
        Locked,
        Unknown
    };

    Lock(serialib&);

    State get_state() const;

    bool set_state(State);
    
private:
    serialib& port;
    State state = State::Unknown;
};
