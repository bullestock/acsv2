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

    void timed_unlock();
    
private:
    void thread_body();

    static void set_pin_output(int pin);

    static void set_pin(int pin, bool value);

    std::thread thread;
    bool stop = false;
    bool verbose = false;
    mutable std::mutex mutex;
    State state = State::locked;
    bool door_is_open = false;
    util::time_point timed_unlock_start = util::invalid_time_point();
};
