#pragma once

#include "serialib.h"
#include "util.h"

#include <mutex>
#include <thread>

class Lock
{
public:
    enum class State {
        open,
        locked,
        // open_locked,
        // open_unlocked,
    };

    struct Status
    {
        State state = State::locked;
        bool door_is_open = false;

        bool operator==(const Status&) const = default;
    };

    Lock();

    ~Lock();

    Status get_status() const;

    bool set_state(State);
    
private:
    void thread_body();

    std::thread thread;
    bool stop = false;
    bool verbose = false;
    mutable std::mutex mutex;
    State state = State::locked;
    bool door_is_open = false;
};
