#pragma once

#include <string>

#include <fmt/core.h>

#include "lock.h"
#include "util.h"

class Card_reader;
class Display;
class Slack_writer;

class Controller
{
public:
    Controller(Slack_writer& slack,
               Display& display,
               Card_reader& reader,
               Lock& lock);

    static Controller& instance();

    void run();
    
    void log(const std::string&);

    template <typename... Args>
    void log(const std::string& fmt_string, Args... args)
    {
        return log(fmt::format(fmt_string, std::forward<Args>(args)...));
    }
    
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
    
    bool check_card(const std::string& card_id);
    bool is_it_thursday() const;
    void check_thursday();
    bool ensure_lock_state(Lock::State state);
    void fatal_lock_error(const std::string& msg);

    static Controller* the_instance;
    Display& display;
    Card_reader& reader;
    Lock& lock;
    Slack_writer& slack;
    State state = State::initial;
    bool door_is_open = false;
    bool handle_is_raised = false;
    bool green_pressed = false;
    bool red_pressed = false;
    bool white_pressed = false;
    bool leave_pressed = false;
    bool card_swiped = false;
    bool simulate = false;
    Lock::State last_lock_status = Lock::State::unknown;
    std::string card_id;
    std::string who;
    util::duration timeout_dur = util::invalid_duration();
    util::time_point timeout = util::invalid_time_point();
    std::string status, slack_status;
};
