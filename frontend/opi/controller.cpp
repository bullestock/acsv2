#include "controller.h"

#include "cardreader.h"
#include "display.h"
#include "lock.h"
#include "logger.h"
#include "slack.h"

#include <date/date.h>

#include <fmt/chrono.h>

#include <magic_enum.hpp>

static constexpr auto UNLOCKED_ALERT_INTERVAL = std::chrono::seconds(30);

// How long to keep the door open after valid card is presented
static constexpr auto ENTER_TIME = std::chrono::seconds(30);

// How long to wait before locking when door is closed after leaving
static constexpr auto LEAVE_TIME = std::chrono::seconds(5);

static constexpr auto TEMP_STATUS_SHOWN_FOR = std::chrono::seconds(10);

// Time door is unlocked after pressing Green
static constexpr auto UNLOCK_PERIOD = std::chrono::minutes(15);
static constexpr auto UNLOCK_WARN = std::chrono::minutes(5);
static constexpr auto GW_UNLOCK_PERIOD = std::chrono::seconds(30);

// Time before warning when entering
static constexpr auto ENTER_UNLOCKED_WARN = std::chrono::minutes(5);

Controller* Controller::the_instance = nullptr;

Controller::Controller(Slack_writer& s,
                       Display& d,
                       Card_reader& r,
                       Lock& l)
    : slack(s),
      display(d),
      reader(r),
      lock(l)
{
    the_instance = this;
}

Controller::~Controller()
{
}

Controller& Controller::instance()
{
    return *the_instance;
}

bool Controller::exists()
{
    return the_instance != nullptr;
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

    // Allow lock to update status
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    util::time_point last_gateway_update;
    while (1)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Get input
        const auto status = lock.get_status();
        door_is_open = status.door_is_open;
        handle_is_raised = status.handle_is_raised;
        
        keys = read_keys();

        card_id = reader.get_and_clear_card_id();
        if (!card_id.empty())
            Logger::instance().log(fmt::format("Card {} swiped", card_id));

        bool gateway_update_needed = false;
        if (status != last_lock_status)
        {
            Logger::instance().log(fmt::format("Lock status {} door {} handle {} pos {}",
                                               magic_enum::enum_name(status.state),
                                               door_is_open ? "open" : "closed",
                                               handle_is_raised ? "raised" : "lowered",
                                               status.encoder_pos));
            last_lock_status = status;
            gateway_update_needed = true;
        }
        if (!util::is_valid(last_gateway_update) ||
            util::now() - last_gateway_update > std::chrono::seconds(15))
            gateway_update_needed = true;

        if (gateway_update_needed)
        {
            update_gateway();
            last_gateway_update = util::now();
        }

        // Handle state
        auto it = state_map.find(state);
        if (it == state_map.end())
            fatal_error(fmt::format("Unhandled state {}", static_cast<int>(state)));
        const auto old_state = state;
        it->second(this);

        if (state != old_state)
            Logger::instance().log(fmt::format("STATE: {}", magic_enum::enum_name(state)));
        if (keys.red || keys.white || keys.green || keys.leave)
            Logger::instance().log(fmt::format("KEYS: R{:d}W{:d}G{:d}L{:d}",
                                               keys.red, keys.white, keys.green, keys.leave));
        if (util::is_valid(timeout_dur))
        {
            Logger::instance().log(fmt::format("Set timeout of {}", std::chrono::duration_cast<std::chrono::seconds>(timeout_dur)));
            timeout = util::now() + timeout_dur;
            timeout_dur = util::invalid_duration();
        }
    }
}

void Controller::handle_initial()
{
    reader.set_pattern(Card_reader::Pattern::ready);
    if (door_is_open)
    {
        Logger::instance().log("Door is open, wait");
        state = State::wait_for_close;
    }
    else if (!handle_is_raised)
    {
        Logger::instance().log("Handle is not raised, wait");
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
    if (keys.green || door_is_open)
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
    if (last_lock_status.state != Lock::State::locked)
    {
        status = "Unknown";
        slack_status = ":unlock: Door has been unlocked manually";
        if (keys.red)
        {
            Logger::instance().log("Red pressed");
            state = State::locking;
        }
    }
    display.set_status(status, Display::Color::orange);
    slack.set_status(slack_status);
    if (keys.white)
        check_thursday();
    else if (keys.green)
    {
        Logger::instance().log("Green pressed");
        state = State::timed_unlocking;
    }
    else if (!card_id.empty())
    {
        check_card(card_id, true);
    }
    else if (keys.leave)
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
        is_space_open = false;
    }
    else if (keys.red)
    {
        slack.announce_closed();
        is_space_open = false;
        if (door_is_open || !handle_is_raised)
            state = State::wait_for_close;
        else
            state = State::locking;
    }
    // Allow scanning new cards while open
    if (!card_id.empty())
        check_card(card_id, false);
}

void Controller::handle_opening()
{
    display.set_status("Unlocking", Display::Color::blue);
    if (ensure_lock_state(Lock::State::open))
    {
        state = State::open;
        reader.set_pattern(Card_reader::Pattern::open);
        is_space_open = true;
    }
    else
        fatal_lock_error("could not unlock the door");
}

void Controller::handle_timed_unlock()
{
    if (keys.red || (util::is_valid(timeout) && util::now() >= timeout))
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
            if (keys.green)
            {
                state = State::timed_unlock;
                timeout_dur = UNLOCK_PERIOD;
            }
        }
        else
            state = State::locking;
    }
    if (keys.leave)
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
    else if (keys.leave)
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
    if (keys.green)
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
            reader.set_sound(Card_reader::Sound::none);
            Logger::instance().log("Stopping beep");
        }
    }
    if (keys.white)
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
    else if (keys.white)
        check_thursday();
    else if (keys.green)
        state = State::timed_unlocking;
    else if (keys.red)
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
    else if (keys.green)
        state = State::timed_unlocking;
    else if (keys.white)
        check_thursday();
    else if (door_is_open)
        state = State::wait_for_enter;
    else if (handle_is_raised)
    {
        reader.set_sound(Card_reader::Sound::none);
        Logger::instance().log("Stopping beep");
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
            reader.set_sound(Card_reader::Sound::warn_closing);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            state = State::locking;
            Logger::instance().log("Stopping beep");
            reader.set_sound(Card_reader::Sound::none);
        }
    }
    else if (door_is_open)
        state = State::wait_for_close;
}

void Controller::handle_wait_for_leave_unlock()
{
    Logger::instance().log("Start beeping");
    reader.set_sound(Card_reader::Sound::warning);
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
    if (keys.red || (util::is_valid(timeout) && (util::now() >= timeout)))
        timeout = util::invalid_time_point();
    else if (keys.green)
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

bool Controller::is_it_thursday() const
{
    const auto today = date::floor<date::days>(util::now());
    return date::weekday(today) == date::Thursday;
}

void Controller::check_thursday()
{
    if (!is_it_thursday())
    {
        display.show_message("It is not Thursday yet", Display::Color::red);
        return;
    }
    state = State::opening;
    slack.announce_open();
}

Buttons::Keys Controller::read_keys(bool log)
{
    return buttons.read();
}

void Controller::check_card(const std::string& card_id, bool change_state)
{
    switch (card_cache.has_access(card_id))
    {
    case Card_cache::Access::Allowed:
        if (change_state)
        {
            reader.set_pattern(Card_reader::Pattern::enter);
            slack.send_message(":key: Valid card swiped, unlocking");
            state = State::unlocking;
            timeout_dur = ENTER_TIME;
        }
        else
            slack.send_message(":key: Valid card swiped while open");
        break;
            
    case Card_cache::Access::Forbidden:
        slack.send_message(":bandit: Unauthorized card swiped");
        Logger::instance().log(fmt::format("Unauthorized card {} swiped", card_id));
        break;
            
    case Card_cache::Access::Unknown:
        slack.send_message(fmt::format(":broken_key: Unknown card {} swiped", card_id));
        Logger::instance().log_unknown_card(card_id);
        break;
               
    case Card_cache::Access::Error:
        slack.send_message(":computer_rage: Internal error checking card");
        break;
    }
}

bool Controller::ensure_lock_state(Lock::State desired_state)
{
    if (last_lock_status.state == Lock::State::unknown)
    {
        if (!simulate)
            reader.set_sound(Card_reader::Sound::uncalibrated);
        if (last_lock_status.door_is_open)
        {
            Logger::instance().log("Door is open, not calibrating");
            return true;
        }
        if (!last_lock_status.handle_is_raised)
        {
            Logger::instance().log("Handle is lowered, not calibrating");
            return true;
        }
        display.set_status("CALIBRATING", Display::Color::red);
        const std::string msg = "Calibrating lock";
        Logger::instance().log(msg);
        slack.send_message(fmt::format(":calibrating: {}", msg));
        if (!calibrate())
            return false;
        display.set_status("CALIBRATED", Display::Color::blue);
        last_lock_status.state = Lock::State::open;
    }
    switch (desired_state)
    {
    case Lock::State::locked:
        if (last_lock_status.state == Lock::State::locked)
            return true;
        if (last_lock_status.state == Lock::State::open)
        {
            const auto ok = lock.set_state(Lock::State::locked);
            if (ok)
                return true;
            Logger::instance().log(fmt::format("ERROR: Cannot lock the door: '{}'", lock.get_error_msg()));
            //!! error handling
        }
        return false;

    case Lock::State::open:
        if (last_lock_status.state == Lock::State::open)
            return true;
        if (last_lock_status.state == Lock::State::locked)
        {
            const auto ok = lock.set_state(Lock::State::open);
            if (ok)
                return true;
            Logger::instance().log(fmt::format("ERROR: Cannot unlock the door: '{}'", lock.get_error_msg()));
            //!! error handling
        }
        return false;

    default:
        fatal_error(fmt::format("BAD LOCK STATE: {}", magic_enum::enum_name(last_lock_status.state)));
        return false;
    }
}

bool Controller::calibrate()
{
    const auto ok = lock.calibrate();
    if (!ok)
        fatal_lock_error("could not calibrate the lock"); // noreturn
    return ok;
}

void Controller::fatal_lock_error(const std::string& msg)
{
    auto message(msg);
    if (!simulate)
        message = fmt::format("{}: {}", msg, lock.get_error_msg());
    fatal_error(fmt::format("LOCK ERROR: {}", message));
}

void Controller::update_gateway()
{
    util::json status;
    status["Encoder position"] = last_lock_status.encoder_pos;
    status["handle"] = last_lock_status.handle_is_raised ? "raised" : "lowered";
    status["door"] = last_lock_status.door_is_open ? "open" : "closed";
    status["space"] = is_space_open ? "open" : "closed";
    status["Lock status"] = magic_enum::enum_name(last_lock_status.state);
    const auto [locked_range, unlocked_range] = lock.get_ranges();
    if (locked_range.first != locked_range.second &&
        unlocked_range.first != unlocked_range.second)
    {
        status["Locked range"] = fmt::format("({}, {})", locked_range.first, locked_range.second);
        status["Unlocked range"] = fmt::format("({}, {})", unlocked_range.first, unlocked_range.second);
    }

    gateway.set_status(status);
    const auto action = gateway.get_action();
    if (action.empty())
        return;
    
    Logger::instance().log(fmt::format("Start action '{}'", action));
    if (action == "calibrate")
    {
        if (last_lock_status.state == Lock::State::open)
            slack.send_message(":stop: Door is open, cannot calibrate");
        else if (!last_lock_status.handle_is_raised)
            slack.send_message(":stop: Handle is not raised");
        else
        {
            slack.send_message(":calibrating: Manual calibration initiated");
            display.set_status("MANUAL CALIBRATION IN PROGRESS", Display::Color::red);
            if (calibrate())
            {
                display.set_status("CALIBRATED", Display::Color::blue);
                slack.send_message(":calibrating: :heavy_check_mark: Manual calibration complete");
                state = State::initial;
            }
        }
    }
    else if (action == "lock")
    {
        if (last_lock_status.state == Lock::State::open)
            slack.send_message(":stop: Door is open, cannot lock");
        else if (ensure_lock_state(Lock::State::locked))
        {
            slack.send_message(":lock: Door is locked");
            state = State::locked;
        }
        else
            fatal_lock_error("could not lock the door");
    }
    else if (action == "unlock")
    {
        if (ensure_lock_state(Lock::State::open))
        {
            slack.send_message(":unlock: Door is unlocked");
            state = State::timed_unlock;
            timeout_dur = GW_UNLOCK_PERIOD;
        }
        else
            fatal_lock_error("could not unlock the door");
    }
    else
    {
        Logger::instance().log(fmt::format("Unknown action '{}'", action));
        slack.send_message(fmt::format(":question: Unknown action '{}'", action));
    }
}
