#pragma once

#include "serialib.h"
#include "util.h"

#include <mutex>
#include <thread>

class Lock_base
{
public:
    enum class State {
        open,
        locked,
        unknown
    };

    struct Status
    {
        State state = State::unknown;
        bool door_is_open = false;
        bool handle_is_raised = false;
        int encoder_pos = 0;

        bool operator==(const Status&) const = default;
    };

    virtual ~Lock_base()
    {
    }

    virtual Status get_status() const
    {
        return Status(State::locked, false, true, 0);
    }

    virtual bool set_state(State)
    {
        return true;
    }

    virtual bool calibrate()
    {
        return false;
    }

    
    virtual void set_verbose(bool)
    {
    }

    virtual std::string get_error_msg() const
    {
        return "";
    }

    using Range = std::pair<std::pair<int, int>, std::pair<int, int>>;
    
    virtual Range get_ranges() const
    {
        return Range();
    }
};

class Lock : public Lock_base
{
public:
    Lock(serialib& s);

    ~Lock();

    void set_verbose(bool on) override
    {
        verbose = on;
    }

    Status get_status() const override;

    bool set_state(State) override;

    bool calibrate() override;
    
    std::string get_error_msg() const override;

    std::pair<std::pair<int, int>, std::pair<int, int>> get_ranges() const override;
    
private:
    bool write(const std::string& s);

    std::string get_reply(const std::string& cmd,
                          util::duration timeout = std::chrono::seconds(10));

    void thread_body();

    serialib& port;
    std::thread thread;
    bool stop = false;
    bool verbose = false;
    mutable std::mutex mutex;
    State state = State::unknown;
    bool door_is_open = false;
    bool handle_is_raised = false;
    mutable bool last_handle_is_raised = false;
    int encoder_pos = 0;
    std::string last_error;
    std::pair<int, int> locked_range{ 0, 0 };
    std::pair<int, int> unlocked_range{ 0, 0 };
};
