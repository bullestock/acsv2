#include "controller.h"

#include "cardreader.h"
#include "display.h"
#include "foreninglet.h"
#include "format.h"
#include "lock.h"
#include "logger.h"
#include "slack.h"

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
                       TFT_eSPI& d,
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
    state_map[State::locked] = &Controller::handle_locked;
    state_map[State::open] = &Controller::handle_open;
    state_map[State::timed_unlock] = &Controller::handle_timed_unlock;

    util::time_point last_gateway_update;
    while (1)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        set_locked(is_locked);

        // Get input
        door_is_open = false; //!!
        
        keys = read_keys();

        card_id = reader.get_and_clear_card_id();
        if (!card_id.empty())
            Logger::instance().log(format("Card %s swiped", card_id));

        bool gateway_update_needed = false;
        if (status != last_lock_status)
        {
            Logger::instance().log(format("Lock status %s door {}",
                                          locked ? "locked" : "unlocked",
                                          door_is_open ? "open" : "closed"));
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
            fatal_error(format("Unhandled state %d", static_cast<int>(state)));
        const auto old_state = state;
        it->second(this);

        if (state != old_state)
            Logger::instance().log(format("STATE: %d", state));
        if (keys.red || keys.white || keys.green || keys.leave)
            Logger::instance().log(format("KEYS: R%dW%dG%dL%d",
                                          keys.red, keys.white, keys.green, keys.leave));
        if (util::is_valid(timeout_dur))
        {
            Logger::instance().log(format("Set timeout of %d s", timeout_dur));
            timeout = util::now() + timeout_dur;
            timeout_dur = util::invalid_duration();
        }
    }
}

void Controller::handle_initial()
{
    reader.set_pattern(Card_reader::Pattern::ready);
    state = State::locked;
}

void Controller::handle_locked()
{
    is_locked = true;
    reader.set_pattern(Card_reader::Pattern::ready);
    status = "Locked";
    slack_status = ":lock: Door is locked";
    display.set_status(status, TFT_ORANGE);
    slack.set_status(slack_status);
    if (keys.white)
        check_thursday();
    else if (keys.green)
    {
        Logger::instance().log("Green pressed");
        timeout_dur = UNLOCK_PERIOD;
        state = State::timed_unlock;
    }
    else if (!card_id.empty())
    {
        check_card(card_id, true);
    }
    else if (keys.leave)
    {
        state = State::timed_unlock;
        timeout_dur = LEAVE_TIME;
        slack.send_message(":exit: The Leave button has been pressed");
    }
}
            
void Controller::handle_open()
{
    is_locked = false;
    display.set_status("Open", TFT_GREEN);
    if (!is_it_thursday())
    {
        Logger::instance().log("It is no longer Thursday");
        state = State::locked;
        slack.announce_closed();
        is_space_open = false;
    }
    else if (keys.red)
    {
        slack.announce_closed();
        is_space_open = false;
        state = State::locked;
    }
    // Allow scanning new cards while open
    if (!card_id.empty())
        check_card(card_id, false);
}

void Controller::handle_timed_unlock()
{
    is_locked = false;
    if (keys.red || (util::is_valid(timeout) && util::now() >= timeout))
    {
        timeout = util::invalid_time_point();
        state = State::locked;
    }
    else if (keys.white)
        check_thursday();
    else if (util::is_valid(timeout))
    {
        const auto time_left = timeout - util::now();
        if (time_left <= UNLOCK_WARN)
        {
            const auto secs_left = std::chrono::duration_cast<std::chrono::seconds>(time_left).count();
            const auto mins_left = ceil(secs_left/60.0);
            //puts "Left: #{mins_left}m #{secs_left}s"
            const auto s2 = (mins_left > 1) ? format("%d minutes", mins_left) : format("%d seconds", secs_left);
            if (!simulate)
                reader.set_pattern(Card_reader::Pattern::warn_closing);
            display.set_status("Open for "+s2, TFT_ORANGE);
        }
        else            
            display.set_status("Open", TFT_GREEN);
    }
    if (!util::is_valid(timeout))
        state = State::locked;

    if (keys.leave)
    {
        state = State::timed_unlock;
        timeout_dur = LEAVE_TIME;
        slack.send_message(":exit: The Leave button has been pressed");
    }
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
        display.show_message("It is not Thursday yet", TFT_RED);
        return;
    }
    state = State::open;
    slack.announce_open();
}

Buttons::Keys Controller::read_keys(bool log)
{
    return buttons.read();
}

void Controller::check_card(const std::string& card_id, bool change_state)
{
    const auto result = card_cache.has_access(card_id);
    switch (result.access)
    {
    case Card_cache::Access::Allowed:
        if (change_state)
        {
            reader.set_pattern(Card_reader::Pattern::enter);
            slack.send_message(":key: Valid card swiped, unlocking");
            state = State::timed_unlock;
            timeout_dur = ENTER_TIME;
        }
        else
            slack.send_message(":key: Valid card swiped while open");
        break;
            
    case Card_cache::Access::Forbidden:
        slack.send_message(":bandit: Unauthorized card swiped");
        Logger::instance().log(format("Unauthorized card %s swiped", card_id));
        break;
            
    case Card_cache::Access::Unknown:
        slack.send_message(format(":broken_key: Unknown card %s swiped", card_id));
        Logger::instance().log_unknown_card(card_id);
        break;
               
    case Card_cache::Access::Error:
        slack.send_message(":computer_rage: Internal error checking card");
        break;
    }

    ForeningLet::instance().update_last_access(result.user_id, util::now());
}

void Controller::update_gateway()
{
    util::json status;
    status["door"] = door_is_open ? "open" : "closed";
    status["space"] = is_space_open ? "open" : "closed";
    status["lock status"] = is_locked ? "locked" : "unlocked";

    gateway.set_status(status);
    const auto action = gateway.get_action();
    if (action.empty())
        return;
    
    Logger::instance().log(format("Start action '%s'", action));
    if (action == "lock")
    {
        if (last_lock_status.state == Lock::State::open)
            slack.send_message(":stop: Door is open, cannot lock");
        else
        {
            is_locked = true;
            slack.send_message(":lock: Door is locked");
            state = State::locked;
        }
    }
    else if (action == "unlock")
    {
        is_locked = false;
        slack.send_message(":unlock: Door is unlocked");
        state = State::timed_unlock;
        timeout_dur = GW_UNLOCK_PERIOD;
    }
    else
    {
        Logger::instance().log(format("Unknown action '%s'", action));
        slack.send_message(format(":question: Unknown action '%s'", action));
    }
}