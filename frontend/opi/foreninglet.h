#pragma once

#include <functional>
#include <string>
#include <thread>

#include <boost/lockfree/queue.hpp>

#include "util.h"

/// ForeningLet singleton
class ForeningLet
{
public:
    static ForeningLet& instance();

    void update_last_access(int user_id, const util::time_point& timestamp);
    
private:
    ForeningLet();

    ~ForeningLet();

    void thread_body();
    
    struct Item {
        int user_id = 0;
        static const int STAMP_SIZE = 19; // YYYY-mm-dd HH:MM:SS
        char stamp[STAMP_SIZE+1];
    };
    boost::lockfree::queue<Item> q;
    std::string forening_let_user;
    std::string forening_let_password;
    std::thread thread;
    bool stop = false;
};
