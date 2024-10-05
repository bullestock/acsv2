#pragma once

#include <RDM6300.h>

#include "buttons.h"
#include "cardcache.h"
#include "util.h"

#include <string>

class Card_reader;
class Display;

class Controller
{
public:
    Controller(Display& display,
               Card_reader& reader);

    ~Controller() = default;
    
    static Controller& instance();

    static bool exists();

    void run();

    Buttons::Keys read_keys(bool do_log = true);
    
private:
    using Card_id = RDM6300::Card_id;

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

    void check_card(Card_id card_id, bool change_state);
    bool is_it_thursday() const;
    void check_thursday();
    void ensure_lock_state(bool locked);
    void update_gateway();

    static Controller* the_instance;
    Display& display;
    Card_reader& reader;
    Buttons buttons;
    State state = State::initial;
    bool is_main = false;
    bool is_door_open = false;
    bool is_locked = true;
    Buttons::Keys keys;
    bool simulate = false;
    bool is_space_open = false;
    Card_id card_id;
    std::string who;
    util::duration timeout_dur = util::invalid_duration();
    util::time_point timeout = util::invalid_time_point();
};

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
