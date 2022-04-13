#include "controller.h"

#include "cardreader.h"
#include "display.h"
#include "lock.h"
#include "slack.h"

#include <fmt/chrono.h>

#include <magic_enum.hpp>

//static constexpr auto BEEP_INTERVAL = std::chrono::milliseconds(500);
static constexpr auto UNLOCKED_ALERT_INTERVAL = std::chrono::seconds(30);

// How long to keep the door open after valid card is presented
static constexpr auto ENTER_TIME = std::chrono::seconds(30);

// How long to wait before locking when door is closed after leaving
static constexpr auto LEAVE_TIME = std::chrono::seconds(5);

static constexpr auto TEMP_STATUS_SHOWN_FOR = std::chrono::seconds(10);

// Time door is unlocked after pressing Green
static constexpr auto UNLOCK_PERIOD = std::chrono::minutes(15);
static constexpr auto UNLOCK_WARN = std::chrono::minutes(5);

// Time before warning when entering
static constexpr auto ENTER_UNLOCKED_WARN = std::chrono::minutes(5);

Controller::Controller(Slack_writer& s,
                       Display& d,
                       Card_reader& r,
                       Lock& l)
    : slack(s),
      display(d),
      reader(r),
      lock(l)
{
}

void Controller::run()
{
    std::map<State, std::function<void(Controller*)>> state_map;
    state_map[State::initial] = &Controller::handle_initial;
    state_map[State::alert_unlocked] = &Controller::handle_alert_unlocked;
    state_map[State::locked] = &Controller::handle_locked;
    state_map[State::locking] = &Controller::handle_locking;
    state_map[State::open] = &Controller::handle_open;
    state_map[State::opening] = &Controller::handle_opening;
    state_map[State::timed_unlock] = &Controller::handle_timed_unlock;
    state_map[State::timed_unlocking] = &Controller::handle_timed_unlocking;
    state_map[State::unlocked] = &Controller::handle_unlocked;
    state_map[State::unlocking] = &Controller::handle_unlocking;
    state_map[State::wait_for_close] = &Controller::handle_wait_for_close;
    state_map[State::wait_for_enter] = &Controller::handle_wait_for_enter;
    state_map[State::wait_for_handle] = &Controller::handle_wait_for_handle;
    state_map[State::wait_for_leave] = &Controller::handle_wait_for_leave;
    state_map[State::wait_for_leave_unlock] = &Controller::handle_wait_for_leave_unlock;
    state_map[State::wait_for_lock] = &Controller::handle_wait_for_lock;
    state_map[State::wait_for_open] = &Controller::handle_wait_for_open;
    
    while (1)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Get input
        door_is_open = false;
        handle_is_raised = false;
        green_pressed = false;
        red_pressed = false;
        white_pressed = false;
        card_swiped = false;
        card_id = "";

        // Handle state
        auto it = state_map.find(state);
        if (it == state_map.end())
            util::fatal_error(slack, fmt::format("Unhandled state {}", static_cast<int>(state)));
        const auto old_state = state;
        it->second(this);

        if (state != old_state)
            log("STATE: {}", magic_enum::enum_name(state));
        if (util::is_valid(timeout_dur))
        {
            log("Set timeout of {}", timeout_dur);
            timeout = util::now() + timeout_dur;
        }
    }
}

void Controller::handle_initial()
{
    reader.set_pattern(Card_reader::Pattern::ready);
    if (door_is_open)
    {
        log("Door is open, wait");
        state = State::wait_for_close;
    }
    else if (!handle_is_raised)
    {
        log("Handle is not raised, wait");
        state = State::wait_for_close;
    }
    else
        state = State::locking;
}

void Controller::handle_alert_unlocked()
{
    display.set_status("Please close the door and raise the handle", Display::Color::red);
    if (util::is_valid(timeout) && (util::now() >= timeout))
    {
        //!! complain
        timeout_dur = UNLOCKED_ALERT_INTERVAL;
    }
    if (green_pressed || door_is_open)
    {
        state = State::unlocked;
        timeout_dur = UNLOCK_PERIOD;
    }
}

void Controller::handle_locked()
{
    reader.set_pattern(Card_reader::Pattern::ready);
    status = "Locked";
    slack_status = ":lock: Door is locked";
    if (last_lock_status != Lock::State::locked)
    {
        status = "Unknown";
        slack_status = ":unlock: Door has been unlocked manually";
    }
    display.set_status(status, Display::Color::orange);
    slack.set_status(slack_status);
    if (white_pressed)
        check_thursday();
    else if (green_pressed)
    {
        log("Green pressed");
        state = State::timed_unlocking;
        timeout_dur = UNLOCK_PERIOD;
    }
    else if (card_swiped)
    {
        if (check_card(card_id))
        {
            reader.set_pattern(Card_reader::Pattern::enter);
            slack.send_message(":key: Valid card swiped, unlocking");
            card_swiped = false;
            state = State::unlocking;
            timeout_dur = ENTER_TIME;
        }
        else
            slack.send_message(":broken_key: Invalid card swiped");
    }
    else if (leave_pressed)
    {
        state = State::wait_for_leave_unlock;
        slack.send_message(":exit: The Leave button has been pressed");
    }
}
            
void Controller::handle_locking()
{
    display.set_status("Locking", Display::Color::orange);
    if (ensure_lock_state(Lock::State::locked))
        state = State::locked;
    else
        fatal_lock_error("could not lock the door");
}

void Controller::handle_open()
{
    display.set_status("Open", Display::Color::green);
    if (!is_it_thursday())
    {
        state = State::unlocked;
        slack.announce_closed();
    }
    else if (red_pressed)
    {
        slack.announce_closed();
        if (door_is_open || !handle_is_raised)
            state = State::wait_for_close;
        else
            state = State::locking;
    }
    // Allow scanning new cards while open
    if (card_swiped)
        check_card(card_id);
}

void Controller::handle_opening()
{
    display.set_status("Unlocking", Display::Color::blue);
    if (ensure_lock_state(Lock::State::open))
    {
        state = State::open;
        reader.set_pattern(Card_reader::Pattern::open);
    }
    else
        fatal_lock_error("could not unlock the door");
}

void Controller::handle_timed_unlock()
{
    if (red_pressed || (util::is_valid(timeout) && util::now() >= timeout))
    {
        timeout = util::invalid_time_point();
        if (door_is_open || !handle_is_raised)
            display.set_status("Please close the door and raise the handle", Display::Color::red);
        else
            state = State::locking;
    }
    else if (util::is_valid(timeout))
    {
        const auto time_left = timeout - util::now();
        if (time_left <= UNLOCK_WARN)
        {
            const auto secs_left = std::chrono::duration_cast<std::chrono::seconds>(time_left).count();
            const auto mins_left = ceil(secs_left/60.0);
            //puts "Left: #{mins_left}m #{secs_left}s"
            const auto s2 = (mins_left > 1) ? fmt::format("{} minutes", mins_left) : fmt::format("{} seconds", secs_left);
            if (!simulate)
                reader.set_pattern(Card_reader::Pattern::warn_closing);
            display.set_status("Open for "+s2, Display::Color::orange);
        }
        else            
            display.set_status("Open", Display::Color::green);
    }
    if (!util::is_valid(timeout))
    {
        if (door_is_open || !handle_is_raised)
        {
            display.set_status("Please close the door and raise the handle", Display::Color::red);
            if (green_pressed)
            {
                state = State::timed_unlock;
                timeout_dur = UNLOCK_PERIOD;
            }
        }
        else
            state = State::locking;
    }
    if (leave_pressed)
    {
        state = State::wait_for_leave;
        timeout_dur = LEAVE_TIME;
        slack.send_message(":exit: The Leave button has been pressed");
    }
}

void Controller::handle_timed_unlocking()
{
    display.set_status("Unlocking", Display::Color::blue);
    if (ensure_lock_state(Lock::State::open))
    {
        state = State::timed_unlock;
        timeout_dur = UNLOCK_PERIOD;
    }
    else
        fatal_lock_error("could not unlock the door");
}

void Controller::handle_unlocked()
{
    display.set_status("Unlocked", Display::Color::orange);
    if (door_is_open)
        state = State::wait_for_enter;
    else if (!door_is_open)
        state = State::wait_for_handle;
    else if (leave_pressed)
    {
        state = State::wait_for_leave;
        timeout_dur = LEAVE_TIME;
        slack.send_message(":exit: The Leave button has been pressed");
    }
    else if (util::is_valid(timeout) && (util::now() >= timeout))
    {
        state = State::alert_unlocked;
        timeout_dur = UNLOCKED_ALERT_INTERVAL;
    }
}

void Controller::handle_unlocking()
{
    display.set_status("Unlocking", Display::Color::blue);
    if (ensure_lock_state(Lock::State::open))
        state = State::wait_for_open;
    else
        fatal_lock_error("could not unlock the door");
}

void Controller::handle_wait_for_close()
{
    if (green_pressed)
    {
        state = State::timed_unlock;
        timeout_dur = UNLOCK_PERIOD;
    }
    else if (door_is_open)
        display.set_status("Please close the door and raise the handle", Display::Color::red);
    else
    {
        display.set_status("Please raise the handle", Display::Color::red);
        if (handle_is_raised)
        {
            state = State::locking;
            reader.stop_beep();
            log("Stopping beep");
        }
    }
    if (white_pressed)
        check_thursday();
}

void Controller::handle_wait_for_enter()
{
    display.set_status("Enter", Display::Color::blue);
    if (!door_is_open)
    {
        state = State::wait_for_handle;
        timeout_dur = ENTER_UNLOCKED_WARN;
    }
    else if (white_pressed)
        check_thursday();
    else if (green_pressed)
        state = State::timed_unlocking;
    else if (red_pressed)
        state = State::wait_for_close;
}

void Controller::handle_wait_for_handle()
{
    display.set_status("Please raise the handle", Display::Color::blue);
    if (util::is_valid(timeout) && (util::now() >= timeout))
    {
        timeout = util::invalid_time_point();
        state = State::alert_unlocked;
    }
    else if (green_pressed)
        state = State::timed_unlocking;
    else if (white_pressed)
        check_thursday();
    else if (door_is_open)
        state = State::wait_for_enter;
    else if (handle_is_raised)
    {
        reader.stop_beep();
        log("Stopping beep");
        state = State::locking;
    }
}

void Controller::handle_wait_for_leave()
{
    display.set_status("You may leave", Display::Color::blue);
    if (util::is_valid(timeout) && (util::now() >= timeout))
    {
        timeout = util::invalid_time_point();
        if (door_is_open || !handle_is_raised)
            display.set_status("Please close the door and raise the handle", Display::Color::red);
        else
        {
            state = State::locking;
            log("Stopping beep");
            reader.stop_beep();
        }
    }
    else if (door_is_open)
        state = State::wait_for_close;
}

void Controller::handle_wait_for_leave_unlock()
{
    log("Start beeping");
    reader.start_beep();
    display.set_status("Unlocking", Display::Color::blue);
    if (ensure_lock_state(Lock::State::open))
    {
        state = State::wait_for_leave;
        timeout_dur = LEAVE_TIME;
    }
    else
        fatal_lock_error("could not unlock the door");
}

void Controller::handle_wait_for_lock()
{
    if (red_pressed || (util::is_valid(timeout) && (util::now() >= timeout)))
        timeout = util::invalid_time_point();
    else if (green_pressed)
    {
        state = State::timed_unlock;
        timeout_dur = UNLOCK_PERIOD;
    }
    else if (util::is_valid(timeout))
    {
        const auto time_left = timeout - util::now();
        const auto secs_left = std::chrono::duration_cast<std::chrono::seconds>(time_left).count();
        const auto mins_left = ceil(secs_left/60.0);
        const auto s2 = (mins_left > 1) ? fmt::format("{} minutes", mins_left) : fmt::format("{} seconds", secs_left);
        display.set_status("Locking in "+s2, Display::Color::orange);
    }
    if (!util::is_valid(timeout))
    {
        if (door_is_open || !handle_is_raised)
            display.set_status("Please close the door and raise the handle", Display::Color::red);
        else
            state = State::locking;
    }
}

void Controller::handle_wait_for_open()
{
    display.set_status("Enter "+who, Display::Color::blue);
    if (util::is_valid(timeout) && (util::now() >= timeout))
    {
        timeout = util::invalid_time_point();
        if (door_is_open || !handle_is_raised)
            display.set_status("Please close the door and raise the handle", Display::Color::red);
        else
            state = State::locking;
    }
    else if (door_is_open)
        state = State::wait_for_enter;
}

void Controller::log(const std::string& s)
{
    std::cout << fmt::format("{} {}", fmt::gmtime(util::now()), s) << std::endl;
}

bool Controller::is_it_thursday() const
{
    return false; //!!
}

void Controller::check_thursday()
{
    //!!
}

bool Controller::check_card(const std::string& card_id)
{
    return false; //!!
}

