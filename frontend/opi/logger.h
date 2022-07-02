#pragma once

#include <functional>
#include <string>
#include <thread>

#include <boost/lockfree/queue.hpp>

/// Logger singleton
class Logger
{
public:
    static Logger& instance();

    void set_verbose(bool on)
    {
        verbose = on;
    }

    void set_log_to_gateway(bool on)
    {
        log_to_gateway = on;
    }

    void log(const std::string&);

    void log_verbose(const std::string&);

    void log_backend(int user_id, const std::string&);
    
private:
    Logger();

    ~Logger();

    void thread_body();
    
    struct Item {
        enum class Type {
            Debug,
            Backend
        };
        static const int MAX_SIZE = 80;
        static const int STAMP_SIZE = 19; // YYYY-mm-dd HH:MM:SS
        Type type = Type::Debug;
        int user_id = 0;
        char stamp[STAMP_SIZE+1];
        char text[MAX_SIZE+1];
    };
    boost::lockfree::queue<Item> q;
    std::string gw_token;
    std::string api_token;
    bool verbose = false;
    bool log_to_gateway = false;
    std::thread thread;
    bool stop = false;
};

// Needed in main(), so we just use a global function
extern void fatal_error(const std::string& msg);
