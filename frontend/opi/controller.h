#pragma once

#include "lock.h"
#include "util.h"

#include <string>

#include <fmt/core.h>

#include <gpiod.hpp>

class Card_reader;
class Display;
class Slack_writer;

class Controller
{
public:
    Controller(bool verbose,
               Slack_writer& slack,
               Display& display,
               Card_reader& reader,
               Lock& lock);

    static Controller& instance();

    void run();
    
    void log(const std::string&);
    void log_verbose(const std::string&);

private:
    enum class State {
        initial,
        alert_unlocked,
        locked,
        locking,
        open,
        opening,
        timed_unlock,
        timed_unlocking,
        unlocked,
        unlocking,
        wait_for_close,
        wait_for_enter,
        wait_for_handle,
        wait_for_leave,
        wait_for_leave_unlock,
        wait_for_lock,
        wait_for_open,
    };

    struct Keys
    {
        bool red = false;
        bool white = false;
        bool green = false;
        bool leave = false;
    };

    void handle_initial();
    void handle_alert_unlocked();
    void handle_locked();
    void handle_locking();
    void handle_open();
    void handle_opening();
    void handle_timed_unlock();
    void handle_timed_unlocking();
    void handle_unlocked();
    void handle_unlocking();
    void handle_wait_for_close();
    void handle_wait_for_enter();
    void handle_wait_for_handle();
    void handle_wait_for_leave();
    void handle_wait_for_leave_unlock();
    void handle_wait_for_lock();
    void handle_wait_for_open();

    void set_pin_input(int pin);
    bool read_pin(int pin);
    Keys read_keys();
    bool check_card(const std::string& card_id);
    bool is_it_thursday() const;
    void check_thursday();
    bool ensure_lock_state(Lock::State state);
    bool calibrate();
    void fatal_lock_error(const std::string& msg);
    void fatal_error(const std::string& msg);
    void update_gateway();

    static Controller* the_instance;
    bool verbose = false;
    Display& display;
    Card_reader& reader;
    Lock& lock;
    Slack_writer& slack;
    gpiod::chip chip;
    State state = State::initial;
    bool door_is_open = false;
    bool handle_is_raised = false;
    bool green_pressed = false;
    bool red_pressed = false;
    bool white_pressed = false;
    bool leave_pressed = false;
    bool simulate = false;
    Lock::Status last_lock_status;
    std::string card_id;
    std::string who;
    util::duration timeout_dur = util::invalid_duration();
    util::time_point timeout = util::invalid_time_point();
    std::string status, slack_status;
};
