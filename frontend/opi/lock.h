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

    std::string get_error_msg() const;
    
private:
    serialib& port;
    State state = State::unknown;
    std::string last_error;
};
