#pragma once

#include "serialib.h"
#include "logger.h"

#include <mutex>
#include <thread>

class Lock
{
public:
    enum class State {
        open,
        locked,
        unknown
    };

    Lock(serialib& s);

    ~Lock();

    void set_logger(Logger& l);

    struct Status
    {
        State state = State::unknown;
        bool door_is_open = false;
        bool handle_is_raised = false;
        int encoder_pos = 0;

        bool operator==(const Status&) const = default;
    };

    Status get_status() const;

    bool set_state(State);

    bool calibrate();
    
    std::string get_error_msg() const;

    std::pair<std::pair<int, int>, std::pair<int, int>> get_ranges() const;
    
private:
    bool write(const std::string& s);

    std::string get_reply(const std::string& cmd);

    void thread_body();

    serialib& port;
    Logger* logger = nullptr;
    std::thread thread;
    bool stop = false;
    mutable std::mutex mutex;
    State state = State::unknown;
    bool door_is_open = false;
    bool handle_is_raised = false;
    int encoder_pos = 0;
    std::string last_error;
    std::pair<int, int> locked_range{ 0, 0 };
    std::pair<int, int> unlocked_range{ 0, 0 };
};
