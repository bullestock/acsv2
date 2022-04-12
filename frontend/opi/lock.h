#pragma once

#include "serialib.h"

class Lock
{
public:
    enum class State {
        open,
        locked,
        unknown
    };

    Lock(serialib&);

    State get_state() const;

    bool set_state(State);
    
private:
    serialib& port;
    State state = State::unknown;
};
