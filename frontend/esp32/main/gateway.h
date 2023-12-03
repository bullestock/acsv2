#pragma once

#include <mutex>
#include <string>

#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_system.h"

#include <cJSON.h>

extern "C" void gw_task(void*);

class Gateway
{
public:
    static Gateway& instance();

    void set_token(const std::string& _token);

    // non-blocking
    void set_status(const cJSON* status);

    // non-blocking
    std::string get_and_clear_action();

private:
    void thread_body();

    bool post_status();
    void check_action();

    bool stop = false;
    std::mutex mutex;
    std::string token;
    std::string current_status;
    std::string current_action;

    friend void gw_task(void*);
};
