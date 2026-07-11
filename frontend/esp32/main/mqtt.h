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
    
    /// Get list of open doors
    std::string get_open_doors();

private:
    Mqtt() = default;

    ~Mqtt() = default;

    static void event_handler(void* handler_args,
                              esp_event_base_t base,
                              int32_t event_id,
                              void* event_data);

    void handle_data(const std::string& topic,
                     const std::string& data);
    
    bool connected = false;
    esp_mqtt_client_handle_t client = 0;
    // device -> (door open, unlocked)
    std::map<std::string, std::pair<bool, bool>> door_status;
    std::mutex door_status_mutex;
};

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
