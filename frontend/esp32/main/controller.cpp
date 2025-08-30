#include "controller.h"

#include "cJSON.h"

#include <random>
#include <time.h>

#include "cardreader.h"
#include "defs.h"
#include "display.h"
#include "foreninglet.h"
#include "format.h"
#include "gateway.h"
#include "hw.h"
#include "logger.h"
#include "nvs.h"
#include "slack.h"

#include "esp_random.h"

#ifdef DEBUG_HEAP

#include "esp_heap_trace.h"

#define NUM_RECORDS 100
static heap_trace_record_t trace_record[NUM_RECORDS]; // This buffer must be in internal RAM

#endif // DEBUG_HEAP

static constexpr auto UNLOCKED_ALERT_INTERVAL = std::chrono::seconds(30);

// How long to keep the door open after valid card is presented
static constexpr auto ENTER_TIME = std::chrono::seconds(6);

// How long to wait before locking when door is closed after leaving
static constexpr auto LEAVE_TIME = std::chrono::seconds(3);

static constexpr auto TEMP_STATUS_SHOWN_FOR = std::chrono::seconds(10);

// Time door is unlocked after pressing Green
static constexpr auto UNLOCK_PERIOD = std::chrono::minutes(15);
static constexpr auto UNLOCK_WARN = std::chrono::minutes(10);
// Time door is unlocked via Slack
static constexpr auto GW_UNLOCK_PERIOD = std::chrono::seconds(30);

// Time before warning when entering
static constexpr auto ENTER_UNLOCKED_WARN = std::chrono::minutes(5);

Controller* Controller::the_instance = nullptr;

Controller::Controller(Display& d,
                       Card_reader& r)
    : display(d),
      reader(r)
{
    the_instance = this;
    time_t now;
    time(&now);
    Logger::make_timestamp(now, boot_timestamp);
#ifdef DEBUG_HEAP
    ESP_ERROR_CHECK(heap_trace_init_standalone(trace_record, NUM_RECORDS));
#endif
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

    display.clear();
    is_main = get_identifier() == std::string("main");

#ifdef DEBUG_HEAP
    ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));
    int loops = 0;
#endif
    
    util::time_point last_gateway_update = util::now() - std::chrono::minutes(1);
    bool last_is_locked = false;
    bool last_is_door_open = false;
    const auto start_time = util::now();

    std::default_random_engine generator(esp_random()); // HW RNG seed
    std::uniform_int_distribution<int> distribution(10, 40);
    int reboot_minute = distribution(generator);
        
#ifdef SIMULATE_UNKNOWN_CARD
    int uk_count = 0;
#endif
    while (1)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        const auto current_time = util::now();

        display.update();

        set_relay(!is_locked);

        // Get input
        is_door_open = get_door_open();
        
        keys = read_keys();

        card_id = reader.get_and_clear_card_id();
        if (card_id)
            Logger::instance().log(format("Card " CARD_ID_FORMAT " swiped", card_id));

        bool gateway_update_needed = false;
        if ((is_locked != last_is_locked) || (is_door_open != last_is_door_open))
        {
            Logger::instance().log(format("Lock status %s door %s",
                                          is_locked ? "locked" : "unlocked",
                                          is_door_open ? "open" : "closed"));
            last_is_locked = is_locked;
            last_is_door_open = is_door_open;
            gateway_update_needed = true;
        }
        if (!util::is_valid(last_gateway_update) ||
            current_time - last_gateway_update > std::chrono::seconds(15))
            gateway_update_needed = true;

        if (gateway_update_needed)
        {
            update_gateway();
            last_gateway_update = current_time;
        }

        // Handle state
        auto it = state_map.find(state);
        if (it == state_map.end())
            fatal_error(format("Unhandled state %d", static_cast<int>(state)).c_str());
        const auto old_state = state;
        it->second(this);

        if (state != old_state)
            printf("New state: %d\n", static_cast<int>(state));
        if (util::is_valid(timeout_dur))
        {
            Logger::instance().log(format("Set timeout of %d s",
                                          std::chrono::duration_cast<std::chrono::seconds>(timeout_dur).count()));
            timeout = current_time + timeout_dur;
            timeout_dur = util::invalid_duration();
        }

#ifdef DEBUG_HEAP
        ++loops;
        if (loops > 10000)
        {
            ESP_ERROR_CHECK(heap_trace_stop());
            heap_trace_dump();
        }
#endif // DEBUG_HEAP

#ifdef SIMULATE_UNKNOWN_CARD
        ++uk_count;
        if (!(uk_count % 1024))
            printf("UK %d\n", uk_count);
        if (uk_count > 1000)
        {
            uk_count = 0;
            printf("UNKNOWN\n");
            Logger::instance().log_unknown_card(0x1234567890);
        }
#endif

        const auto since_start = current_time - start_time;
        if (since_start > std::chrono::minutes(15))
        {
            time_t t;
            time(&t);
            struct tm tm;
            gmtime_r(&t, &tm);
            if (tm.tm_hour == 2 && tm.tm_min == reboot_minute)
            {
                Logger::instance().log("Scheduled reboot");
                display.set_status("Rebooting", TFT_RED);
                display.update();
                vTaskDelay(60000 / portTICK_PERIOD_MS);
                esp_restart();
            }
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
    std::string aux_status;
    if (is_main)
    {
        const auto open_doors = Gateway::instance().get_open_doors();
        if (!open_doors.empty())
            aux_status = format("Open: %s", open_doors.c_str());
    }
    display.set_status("Locked", TFT_ORANGE, aux_status, TFT_RED);
    Slack_writer::instance().set_status(format(":lock: (%s) Door is locked",
                                               get_identifier().c_str()));
    if (keys.white)
        check_thursday();
    else if (keys.green)
    {
        Logger::instance().log("Green pressed");
        timeout_dur = UNLOCK_PERIOD;
        state = State::timed_unlock;
    }
    else if (card_id)
    {
        check_card(card_id, true);
    }
    else if (keys.leave)
    {
        is_locked = false;
        set_relay(true);
        state = State::timed_unlock;
        timeout_dur = LEAVE_TIME;
        Slack_writer::instance().send_message(format(":exit: (%s) The Leave button has been pressed",
                                                     get_identifier().c_str()));
    }
}
            
void Controller::handle_open()
{
    is_locked = false;
    std::string aux_status;
    if (is_main)
    {
        const auto open_doors = Gateway::instance().get_open_doors();
        if (!open_doors.empty())
            aux_status = format("Open: %s", open_doors.c_str());
    }
    display.set_status("Open", TFT_GREEN, aux_status, TFT_RED);
    if (!is_it_thursday())
    {
        Logger::instance().log("It is no longer Thursday");
        state = State::locked;
        if (is_main)
            Slack_writer::instance().announce_closed();
        is_space_open = false;
    }
    else if (keys.red)
    {
        if (is_main)
            Slack_writer::instance().announce_closed();
        is_space_open = false;
        state = State::locked;
    }
    // Allow scanning new cards while open
    if (card_id)
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
        if (time_left > std::chrono::seconds(10))
        {
            const int secs_left = std::chrono::duration_cast<std::chrono::seconds>(time_left).count();
            const int mins_left = ceil(secs_left/60.0);
            const auto s2 = (mins_left > 1) ? format("%d minutes", mins_left) : format("%d seconds", secs_left);
            auto colour = TFT_BLUE;
            if (time_left <= UNLOCK_WARN)
            {
                if (!simulate)
                    reader.set_pattern(Card_reader::Pattern::warn_closing);
                colour = TFT_ORANGE;
            }
            display.set_status("Open for\n"+s2, colour);
        }
    }
    if (!util::is_valid(timeout))
        state = State::locked;
}

bool Controller::is_it_thursday() const
{
    if (Gateway::instance().get_allow_open())
        return true;
    return util::is_it_thursday();
}

void Controller::check_thursday()
{
    if (!is_it_thursday())
    {
        display.show_message("It is not\nThursday yet", TFT_RED);
        return;
    }
    state = State::open;
    if (is_main)
        Slack_writer::instance().announce_open();
    is_space_open = true;
}

Buttons::Keys Controller::read_keys(bool log)
{
    return buttons.read();
}

void Controller::check_card(Card_id card_id, bool change_state)
{
    const auto result = Card_cache::instance().has_access(card_id);
    switch (result.access)
    {
    case Card_cache::Access::Allowed:
        if (change_state)
        {
            is_locked = false;
            set_relay(true);
            display.show_message("Valid card swiped");
            reader.set_pattern(Card_reader::Pattern::enter);
            Slack_writer::instance().send_message(format(":key: (%s) Valid card " CARD_ID_FORMAT " swiped, unlocking",
                                                         get_identifier().c_str(), card_id));
            Slack_writer::instance().send_message(format("Somebody just entered the %s",
                                                         get_descriptor().c_str()),
                                                  { .info = true });
            state = State::timed_unlock;
            timeout_dur = ENTER_TIME;
        }
        else
            Slack_writer::instance().send_message(format(":key: (%s) Valid card " CARD_ID_FORMAT " swiped while open",
                                                         get_identifier().c_str(), card_id));
        ForeningLet::instance().update_last_access(result.user_id, util::now());
        break;
            
    case Card_cache::Access::Forbidden:
        display.show_message(format("Blocked card " CARD_ID_FORMAT " swiped", card_id), TFT_YELLOW);
        Slack_writer::instance().send_message(format(":bandit: (%s) Unauthorized card swiped",
                                                     get_identifier().c_str()));
        Logger::instance().log(format("Unauthorized card " CARD_ID_FORMAT " swiped", card_id));
        break;
            
    case Card_cache::Access::Unknown:
        display.show_message(format("Unknown card\n" CARD_ID_FORMAT "\nswiped", card_id), TFT_YELLOW);
        Slack_writer::instance().send_message(format(":broken_key: (%s) Unknown card " CARD_ID_FORMAT " swiped",
                                                     get_identifier().c_str(), card_id));
        Logger::instance().log_unknown_card(card_id);
        break;
               
    case Card_cache::Access::Error:
        Slack_writer::instance().send_message(format(":computer_rage: (%s) Internal error checking card: %s",
                                                     get_identifier().c_str(), result.error_msg.c_str()));
        break;
    }
}

void Controller::update_gateway()
{
    auto status = cJSON_CreateObject();
    cJSON_wrapper jw(status);
    auto door = cJSON_CreateString(is_door_open ? "open" : "closed");
    cJSON_AddItemToObject(status, "door", door);
    auto space = cJSON_CreateString(is_space_open ? "open" : "closed");
    cJSON_AddItemToObject(status, "space", space);
    auto lock = cJSON_CreateString(is_locked ? "locked" : "unlocked");
    cJSON_AddItemToObject(status, "lock_status", lock);
    const auto nof_overflows = Logger::instance().get_nof_overflows();
    auto nof_overflows_obj = cJSON_CreateNumber(nof_overflows);
    cJSON_AddItemToObject(status, "log_overflows", nof_overflows_obj);
    auto boot_time_string = cJSON_CreateString(boot_timestamp);
    cJSON_AddItemToObject(status, "boot_time", boot_time_string);

    Gateway::instance().set_status(status);

    std::string action, arg;
    std::tie(action, arg) = Gateway::instance().get_and_clear_action();
    if (action.empty())
        return;
    
    Logger::instance().log(format("Start action '%s'", action.c_str()));
    if (action == "lock")
    {
        if (is_door_open)
            Slack_writer::instance().send_message(format(":warning: (%s) Door is open",
                                                         get_identifier().c_str()));
        is_locked = true;
        Slack_writer::instance().send_message(format(":lock: (%s) Door locked remotely",
                                                     get_identifier().c_str()));
        state = State::locked;
    }
    else if (action == "unlock")
    {
        is_locked = false;
        Slack_writer::instance().send_message(format(":unlock: (%s) Door unlocked remotely",
                                                     get_identifier().c_str()));
        state = State::timed_unlock;
        timeout = util::now() + GW_UNLOCK_PERIOD;
    }
    else if (action == "reboot")
    {
        Slack_writer::instance().send_message(format(":power: (%s) Rebooting",
                                                     get_identifier().c_str()));
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        esp_restart();
    }
    else if (action == "setdesc")
    {
        Slack_writer::instance().send_message(format(":star: (%s) Descriptor set",
                                                     get_identifier().c_str()));
        set_descriptor(arg.c_str());
    }
    else
    {
        Logger::instance().log(format("Unknown action '%s'", action.c_str()));
        Slack_writer::instance().send_message(format(":question: (%s) Unknown action '%s'",
                                                     get_identifier().c_str(), action.c_str()));
    }
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
