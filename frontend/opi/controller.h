#pragma once

#include "buttons.h"
#include "cardcache.h"
#include "gateway.h"
#include "lock.h"
#include "util.h"

#include <string>

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

    ~Controller();
    
    static Controller& instance();

    static bool exists();

    void run();

    Buttons::Keys read_keys(bool do_log = true);
    
private:
    enum class State {
        initial,
        locked,
        open,
        timed_unlock,
    };

    void handle_initial();
    void handle_locked();
    void handle_open();
    void handle_timed_unlock();

    void check_card(const std::string& card_id, bool change_state);
    bool is_it_thursday() const;
    void check_thursday();
    void ensure_lock_state(Lock::State state);
    void update_gateway();

    static Controller* the_instance;
    Display& display;
    Card_reader& reader;
    Lock& lock;
    Slack_writer& slack;
    Card_cache card_cache;
    Buttons buttons;
    Gateway gateway;
    State state = State::initial;
    bool door_is_open = false;
    Buttons::Keys keys;
    bool simulate = false;
    Lock::Status last_lock_status;
    bool is_space_open = false;
    std::string card_id;
    std::string who;
    util::duration timeout_dur = util::invalid_duration();
    util::time_point timeout = util::invalid_time_point();
    std::string status, slack_status;
};
