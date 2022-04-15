#pragma once

#include "serialib.h"

#include <atomic>
#include <thread>

class Lock
{
public:
    enum class State {
        open,
        locked,
        unknown
    };

    Lock(serialib&);

    struct Status
    {
        State state = State::unknown;
        bool door_is_open = false;
        bool handle_is_raised = false;
    };

    Status get_status() const;

    bool set_state(State);

    std::string get_error_msg() const;
    
private:
    void thread_body();

    serialib& port;
    std::thread thread;
    std::atomic<State> state = State::unknown;
    std::atomic<bool> door_is_open = false;
    std::atomic<bool> handle_is_raised = false;
    std::atomic<int> encoder_pos = 0;
    std::string last_error;
};
