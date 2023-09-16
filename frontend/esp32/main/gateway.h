#pragma once

#include <mutex>
#include <string>

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
    std::string get_action();

private:
    void thread_body();

    bool post_status(const cJSON* status);
    void check_action();

    std::string do_get_action();

    bool stop = false;
    std::mutex mutex;
    std::string token;
    cJSON* current_status = nullptr;
    std::string current_action;

    friend void gw_task(void*);
};
