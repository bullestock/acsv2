#include "controller.h"

#include "cJSON.h"

#include <random>
#include <time.h>

#include "cardreader.h"
#include "defs.h"
#include "display.h"
#include "format.h"
#include "hw.h"
#include "mqtt.h"
#include "nvs.h"

#include "esp_app_desc.h"
#include "esp_random.h"

#ifdef DEBUG_HEAP

#include "esp_heap_trace.h"

#define NUM_RECORDS 100
static heap_trace_record_t trace_record[NUM_RECORDS]; // This buffer must be in internal RAM

#endif // DEBUG_HEAP

static constexpr const char* TAG = "ctlr";

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
    util::make_timestamp(now, boot_timestamp, true);
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
    is_main = get_is_main() || (get_identifier() == std::string("main"));

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

    set_mqtt_device_status();
    
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
            Mqtt::instance().log(format("Card " CARD_ID_FORMAT " swiped", card_id));

        bool gateway_update_needed = false;
        if ((is_locked != last_is_locked) || (is_door_open != last_is_door_open))
        {
            Mqtt::instance().log(format("Lock status %s door %s",
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
            check_action();
            set_mqtt_device_status();
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
            Mqtt::instance().log(format("Set timeout of %d s",
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
                Mqtt::instance().log("Scheduled reboot");
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
        const auto open_doors = Mqtt::instance().get_open_doors();
        if (!open_doors.empty())
            aux_status = format("Open: %s", open_doors.c_str());
        const auto present_cards = Mqtt::instance().get_present_cards();
        if (!present_cards.empty())
        {
            if (!aux_status.empty())
                aux_status += "\n";
            aux_status = format("Cards forgot in: %s", present_cards.c_str());
        }
    }
    display.set_status("Locked", TFT_ORANGE, aux_status, TFT_RED);
    Mqtt::instance().set_slack_status(":lock: Door is locked");

    if (keys.white)
        check_thursday();
    else if (keys.green)
    {
        Mqtt::instance().log("Green pressed");
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
        Mqtt::instance().write_slack(":exit: The Leave button has been pressed");
    }
}
            
void Controller::handle_open()
{
    is_locked = false;
    std::string aux_status;
    if (is_main)
    {
#if 0
        const auto open_doors = Gateway::instance().get_open_doors();
        if (!open_doors.empty())
            aux_status = format("Open: %s", open_doors.c_str());
#endif
    }
    display.set_status("Open", TFT_GREEN, aux_status, TFT_RED);
    if (!is_it_thursday())
    {
        Mqtt::instance().log("It is no longer Thursday");
        state = State::locked;
        if (is_main)
        {
            Mqtt::instance().slack_announce_closed();
            set_mqtt_space_status("closed");
        }
        is_space_open = false;
    }
    else if (keys.red)
    {
        if (is_main)
        {
            Mqtt::instance().slack_announce_closed();
            set_mqtt_space_status("closed");
        }
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
    if (Mqtt::instance().get_allow_open())
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
    {
        Mqtt::instance().slack_announce_open();
        set_mqtt_space_status("open");
    }
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
            state = State::timed_unlock;
            timeout_dur = ENTER_TIME;
        }
        break;
            
    case Card_cache::Access::Forbidden:
        display.show_message(format("Blocked card " CARD_ID_FORMAT " swiped", card_id), TFT_YELLOW);
        Mqtt::instance().write_slack(":bandit: Unauthorized card swiped", Mqtt::ChannelInfo);
        Mqtt::instance().log(format("Unauthorized card " CARD_ID_FORMAT " swiped", card_id));
        break;
            
    case Card_cache::Access::Unknown:
        display.show_message(format("Unknown card\n" CARD_ID_FORMAT "\nswiped", card_id), TFT_YELLOW);
        Mqtt::instance().write_slack(format(":broken_key: Unknown card " CARD_ID_FORMAT " swiped",
                                            get_identifier().c_str(), card_id), Mqtt::ChannelInfo);
        Mqtt::instance().log_unknown_card(card_id);
        break;
               
    case Card_cache::Access::Error:
        Mqtt::instance().write_slack(format(":computer_rage: Internal error checking card: %s",
                                            result.error_msg.c_str()), Mqtt::ChannelInfo);
        break;
    }
}

void Controller::set_mqtt_device_status()
{
    char timestamp[util::TIMESTAMP_SIZE];
    util::make_timestamp(timestamp, true);
    auto payload = cJSON_CreateObject();
    cJSON_wrapper jw(payload);
    auto jtimestamp = cJSON_CreateString(timestamp);
    cJSON_AddItemToObject(payload, "timestamp", jtimestamp);

    auto status = cJSON_CreateObject();
    auto door = cJSON_CreateString(is_door_open ? "open" : "closed");
    cJSON_AddItemToObject(status, "door", door);
    auto space = cJSON_CreateString(is_space_open ? "open" : "closed");
    cJSON_AddItemToObject(status, "space", space);
    auto lock = cJSON_CreateString(is_locked ? "locked" : "unlocked");
    cJSON_AddItemToObject(status, "lock_status", lock);
    auto boot_time_string = cJSON_CreateString(boot_timestamp);
    cJSON_AddItemToObject(status, "boot_time", boot_time_string);

    {
        std::lock_guard<std::mutex> g(card_reader_heartbeat_mutex);
        util::make_timestamp(last_card_reader_heartbeat, timestamp, true);
    }
    auto heartbeat = cJSON_CreateString(timestamp);
    cJSON_AddItemToObject(status, "card_reader_heartbeat", heartbeat);
    auto version = cJSON_CreateString(esp_app_get_description()->version);
    cJSON_AddItemToObject(status, "version", version);
    cJSON_AddItemToObject(payload, "data", status);
    char* data = cJSON_PrintUnformatted(payload);
    if (!data)
    {
        ESP_LOGE(TAG, "cJSON_Print() returned nullptr");
        return;
    }
    cJSON_Print_wrapper pw(data);

    Mqtt::instance().set_status(data);
}

void Controller::set_mqtt_space_status(const char* status)
{
    Mqtt::instance().set_status(status, "space");
}

void Controller::check_action()
{
    std::string action, arg;
    std::tie(action, arg) = Mqtt::instance().get_and_clear_action();
    if (action.empty())
        return;
    
    Mqtt::instance().log(format("Start action '%s'", action.c_str()));
    if (action == "lock")
    {
        if (is_door_open)
            Mqtt::instance().write_slack(":warning: Door is open", Mqtt::ChannelInfo);
        is_locked = true;
        Mqtt::instance().write_slack(":lock: Door locked remotely", Mqtt::ChannelInfo);
        state = State::locked;
    }
    else if (action == "unlock")
    {
        is_locked = false;
        Mqtt::instance().write_slack(":unlock: Door unlocked remotely", Mqtt::ChannelInfo);
        state = State::timed_unlock;
        timeout = util::now() + GW_UNLOCK_PERIOD;
    }
    else if (action == "reboot")
    {
        Mqtt::instance().write_slack(":power: Rebooting", Mqtt::ChannelInfo);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        esp_restart();
    }
    else if (action == "setacstoken")
    {
        Mqtt::instance().write_slack(":secret: ACS token set");
        set_acs_token(arg.c_str());
    }
    else
    {
        Mqtt::instance().log(format("Unknown action '%s'", action.c_str()));
        Mqtt::instance().write_slack(format(":question: Unknown action '%s'",
                                            action.c_str()), Mqtt::ChannelInfo);
    }
}

void Controller::card_reader_heartbeat()
{
    std::lock_guard<std::mutex> g(card_reader_heartbeat_mutex);
    time(&last_card_reader_heartbeat);
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
