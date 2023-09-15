#pragma once

#include "buttons.h"
#include "cardcache.h"
#include "util.h"

#include <string>

class Card_reader;
class TFT_eSPI;
class Slack_writer;

class Controller
{
public:
    Controller(Slack_writer& slack,
               TFT_eSPI& display,
               Card_reader& reader);

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
    void ensure_lock_state(bool locked);
    void update_gateway();

    static Controller* the_instance;
    TFT_eSPI& display;
    Card_reader& reader;
    Slack_writer& slack;
    Card_cache card_cache;
    Buttons buttons;
    State state = State::initial;
    bool is_door_open = false;
    bool is_locked = false;
    Buttons::Keys keys;
    bool simulate = false;
    bool is_space_open = false;
    std::string card_id;
    std::string who;
    util::duration timeout_dur = util::invalid_duration();
    util::time_point timeout = util::invalid_time_point();
    std::string status, slack_status;
};
