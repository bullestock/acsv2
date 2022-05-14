#pragma once

#include "util.h"

#include <mutex>
#include <string>
#include <thread>

class Gateway
{
public:
    Gateway();

    ~Gateway();
    
    // non-blocking
    void set_status(const util::json& status);

    // non-blocking
    std::string get_action();

private:
    void thread_body();

    void post_status(const util::json& status);

    std::string do_get_action();

    std::thread thread;
    bool stop = false;
    std::mutex mutex;
    std::string token;
    util::json current_status;
    std::string current_action;
};
