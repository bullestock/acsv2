#pragma once

#include <map>
#include <mutex>
#include <string>

#include "RDM6300.h"
#include "util.h"

#include "mqtt_client.h"

/// MQTT singleton
class Mqtt
{
public:
    using Card_id = RDM6300::Card_id;

    static Mqtt& instance();

    void set_slack_token(const std::string& token);
    
    /// Connect to MQTT 
    void start(const std::string& mqtt_address);

    /// Log debug message - ends up in /srv/acs/logs on drillpress
    void log(const std::string& msg);

    /// Announce status.
    /// Topic is /hal9k/acs/status/<ident> by default
    void set_status(const char* data,
                    const char* subtopic = nullptr);

    /// Write to panopticon log via gateway
    void log_backend(int user_id, const std::string&);

    /// Add unknown card to panopticon via gateway
    void log_unknown_card(Card_id card_id);

    enum Channel
    {
        ChannelGeneral = 1,
        ChannelInfo = 2,
        ChannelDebug = 4,
    };
    
    /// Send a message to Slack via gateway
    void write_slack(const std::string& msg, Channel channel = ChannelDebug);

    /// Announce status to Slack via gateway
    void set_slack_status(const std::string& status);

    /// Announce space status to Slack via gateway
    void set_space_status(const std::string& status);

    /// Announce open status to Slack via gateway
    void slack_announce_open();
    
    /// Announce closed status to Slack via gateway
    void slack_announce_closed();
    
    /// Get list of open doors
    std::string get_open_doors();

    std::string get_present_cards();
    
    std::pair<std::string, std::string> get_and_clear_action();

    bool get_allow_open() const;

private:
    Mqtt() = default;

    ~Mqtt() = default;

    static void event_handler(void* handler_args,
                              esp_event_base_t base,
                              int32_t event_id,
                              void* event_data);

    void handle_data(const std::string& topic,
                     const std::string& data);

    void handle_status(const std::string& topic,
                       const std::string& data);
    
    void handle_action(const std::string& topic,
                       const std::string& data);
    
    static bool sign(cJSON* payload, const std::string& message);

    static bool check_signature(const cJSON* root);

    bool connected = false;
    esp_mqtt_client_handle_t client = 0;
    std::string slack_token;
    std::string last_status;
    std::string last_space_status;
    // device -> (door open, unlocked)
    std::map<std::string, std::pair<bool, bool>> door_status;
    std::mutex door_status_mutex;
    // device -> card present
    std::map<std::string, bool> card_present_status;
    std::mutex card_present_mutex;
    // action
    mutable std::mutex action_mutex;
    std::string current_action;
    std::string current_action_arg;
    bool allow_open = false;
};

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
