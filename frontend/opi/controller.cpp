#include "controller.h"

#include "cardreader.h"
#include "display.h"
#include "lock.h"
#include "slack.h"

#include <fmt/core.h>
#include <fmt/chrono.h>

static constexpr auto BEEP_INTERVAL = std::chrono::milliseconds(500);
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
        it->second(this);
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
    display.set_status({"Please", "close the", "door and raise", "the handle"}, Display::Color::red);
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
            
#if 0
void Controller::handle_locking
      set_status('Locking', 'orange')
      if ensure_lock_state(lock_status, :locked)
        state = State::locked
      else
        fatal_lock_error("could not lock the door")
      end
void Controller::handle_open
      set_status('Open', 'green')
      if !is_it_thursday?
        state = State::unlocked
        @slack.announce_closed()
      elsif red
        @slack.announce_closed()
        if door_status != 'closed' || handle_status != 'raised'
          state = State::wait_for_close
        else
          state = State::locking
        end
      end
      # Allow scanning new cards while open
      if card_swiped
        check_card(card_id)
      end
void Controller::handle_opening
      set_status('Unlocking', 'blue')
      if ensure_lock_state(lock_status, :unlocked)
        state = State::open
        @reader.advertise_open()
      else
        fatal_lock_error("could not unlock the door")
      end
void Controller::handle_timed_unlock
      if red || (timeout && now >= timeout)
        timeout = nil
        if door_status != 'closed' || handle_status != 'raised'
          set_status(['Please', 'close the', 'door and raise', 'the handle'], 'red')
        else
          state = State::locking
        end
      elsif timeout
        secs_left = (timeout - now).to_i
        if secs_left <= UNLOCK_WARN_S
          mins_left = (secs_left/60.0).ceil
          #puts "Left: #{mins_left}m #{secs_left}s"
          if mins_left > 1
            s2 = "#{mins_left} minutes"
          else
            s2 = "#{secs_left} seconds"
          end
          col = 'orange'
          @reader.warn_closing() if !$simulate
          set_status(['Open for', s2], 'orange')
        else            
          set_status('Open', 'green')
        end
      end
      if !timeout
        if door_status != 'closed' || handle_status != 'raised'
          set_status(['Please', 'close the', 'door and raise', 'the handle'], 'red')
          if green
            state = State::timed_unlock
            timeout_dur = UNLOCK_PERIOD_S
          end
        else
          state = State::locking
        end
      end
      if leave
        state = State::wait_for_leave
        timeout_dur = LEAVE_TIME_SECS
        @slack.send_message(':exit: The Leave button has been pressed')
      end
void Controller::handle_timed_unlocking
      set_status('Unlocking', 'blue')
      if ensure_lock_state(lock_status, :unlocked)
        state = State::timed_unlock
        timeout_dur = UNLOCK_PERIOD_S
      else
        fatal_lock_error("could not unlock the door")
      end
void Controller::handle_unlocked
      set_status('Unlocked', 'orange')
      if door_status == 'opened'
        state = State::wait_for_enter
      elsif door_status == 'closed'
        state = State::wait_for_handle
      elsif leave
        state = State::wait_for_leave
        timeout_dur = LEAVE_TIME_SECS
        @slack.send_message(':exit: The Leave button has been pressed')
      elsif timeout && (Time.now >= timeout)
        state = State::alert_unlocked
        timeout_dur = UNLOCKED_ALERT_INTERVAL_S
      end
void Controller::handle_unlocking
      set_status('Unlocking', 'blue')
      if ensure_lock_state(lock_status, :unlocked)
        state = State::wait_for_open
      else
        fatal_lock_error("could not unlock the door")
      end
void Controller::handle_wait_for_close
      if green
        state = State::timed_unlock
        timeout_dur = UNLOCK_PERIOD_S
      elsif door_status == 'open'
        set_status(['Please close', 'the door', 'and raise', 'the handle'], 'red')
      else
        set_status(['Please raise', 'the handle'], 'red')
        if handle_status == 'raised'
          state = State::locking
          @beep = false
          log("Stopping beep")
        end
      end
      if white
        check_thursday()
      end
void Controller::handle_wait_for_enter
      set_status('Enter', 'blue')
      if door_status == 'closed'
        state = State::wait_for_handle
        timeout_dur = ENTER_UNLOCKED_WARN_SECS
      elsif white
        check_thursday()
      elsif green
        state = State::timed_unlocking
      elsif red
        state = State::wait_for_close
      end
void Controller::handle_wait_for_handle
      set_status(['Please raise', 'the handle'], 'blue')
      if timeout && (now >= timeout)
        timeout = nil
        state = State::alert_unlocked
      elsif green
        state = State::timed_unlocking
      elsif white
        check_thursday()
      elsif door_status == 'open'
        state = State::wait_for_enter
      elsif handle_status == 'raised'
        @beep = false
        log("Stopping beep")
        state = State::locking
      end
void Controller::handle_wait_for_leave
      set_status('You may leave', 'blue')
      if timeout && (now >= timeout)
        timeout = nil
        if door_status != 'closed' || handle_status != 'raised'
          set_status(['Please', 'close the', 'door and raise', 'the handle'], 'red')
        else
          state = State::locking
          log("Stopping beep")
          @beep = false
        end
      elsif door_status == 'open'
        state = State::wait_for_close
      end
void Controller::handle_wait_for_leave_unlock
      log("Start beeping")
      @beep = true
      @last_beep = nil
      set_status('Unlocking', 'blue')
      if ensure_lock_state(lock_status, :unlocked)
        state = State::wait_for_leave
        timeout_dur = LEAVE_TIME_SECS
      else
        fatal_lock_error("could not unlock the door")
      end
void Controller::handle_wait_for_lock
      if red || (timeout && (now >= timeout))
        timeout = nil
      elsif green
        state = State::timed_unlock
        timeout_dur = UNLOCK_PERIOD_S
      elsif timeout
        secs_left = (timeout - now).to_i
        mins_left = (secs_left/60.0).ceil
        if mins_left > 1
          s2 = "#{mins_left} minutes"
        else
          s2 = "#{secs_left} seconds"
        end
        col = 'orange'
        set_status(['Locking in', s2], 'orange')
      end
      if !timeout
        if door_status != 'closed' || handle_status != 'raised'
          set_status(['Please', 'close the', 'door and raise', 'the handle'], 'red')
        else
          state = State::locking
        end
      end
void Controller::handle_wait_for_open
      set_status(['Enter', @who], 'blue')
      if timeout && (now >= timeout)
        timeout = nil
        if door_status != 'closed' || handle_status != 'raised'
          set_status(['Please', 'close the', 'door and raise', 'the handle'], 'red')
        else
          state = State::locking
        end
      elsif door_status == 'open'
        state = State::wait_for_enter
      end
    else
      fatal_error('BAD STATE:', state, "unhandled state: #{state}", true)
    end
    if state != old_state
      log("STATE: #{state}")
    end
    if timeout_dur
      log("Set timeout of #{timeout_dur} s")
      timeout = now + timeout_dur
    end
#endif

void Controller::log(const std::string& s)
{
    std::cout << fmt::format("{} {}", fmt::gmtime(util::now()), s) << std::endl;
}

bool Controller::check_thursday() const
{
    return false; //!!
}

bool Controller::check_card(const std::string& card_id)
{
    return false; //!!
}

void Controller::handle_locking() {}
void Controller::handle_open() {}
void Controller::handle_opening() {}
void Controller::handle_timed_unlock() {}
void Controller::handle_timed_unlocking() {}
void Controller::handle_unlocked() {}
void Controller::handle_unlocking() {}
void Controller::handle_wait_for_close() {}
void Controller::handle_wait_for_enter() {}
void Controller::handle_wait_for_handle() {}
void Controller::handle_wait_for_leave() {}
void Controller::handle_wait_for_leave_unlock() {}
void Controller::handle_wait_for_open() {}
