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

    // non-blocking
    std::pair<std::string, std::string> get_and_clear_action();

    // non-blocking
    bool get_allow_open() const;

private:
    void thread_body();

    void check_action(esp_http_client_handle_t client, const char* data, char* buffer);

    bool stop = false;
    mutable std::mutex mutex;
    std::string token;
    std::string current_status;
    std::string current_action;
    std::string current_action_arg;
    bool allow_open = false;

    friend void gw_task(void*);
};

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
