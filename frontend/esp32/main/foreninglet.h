#pragma once

#include <deque>
#include <mutex>
#include <string>

#include "util.h"

extern "C" void foreninglet_task(void*);

/// ForeningLet singleton
class ForeningLet
{
public:
    static ForeningLet& instance();

    void set_credentials(const std::string& username, const std::string& password);
    
    void update_last_access(int user_id, const util::time_point& timestamp);

private:
    ForeningLet() = default;

    ~ForeningLet() = default;

    void thread_body();
    
    struct Item {
        int user_id = 0;
        static const int STAMP_SIZE = 19; // YYYY-mm-dd HH:MM:SS
        char stamp[STAMP_SIZE+1];
    };
    std::deque<Item> q;
    std::mutex mutex;
    std::string forening_let_user;
    std::string forening_let_password;

    friend void foreninglet_task(void*);
};
