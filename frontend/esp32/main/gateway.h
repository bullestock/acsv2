#pragma once

#include <mutex>
#include <string>

#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_system.h"

#include <cJSON.h>

extern "C" void gw_task(void*);

class Display;

class Gateway
{
public:
    static Gateway& instance();

    void set_token(const std::string& _token);

    // blocking
    bool upload_coredump(Display& display);

    // non-blocking
    void set_status(const cJSON* status);

    // non-blocking
    std::pair<std::string, std::string> get_and_clear_action();

    // non-blocking
    bool get_allow_open() const;

    // non-blocking
    void card_reader_heartbeat();
    
private:
    void thread_body();

    bool post_status();
    void check_action();

    bool stop = false;
    std::mutex mutex;
    std::string token;
    std::string current_status;
    std::string current_action;
    std::string current_action_arg;
    bool allow_open = false;
    time_t last_card_reader_heartbeat = 0;

    friend void gw_task(void*);
};
