#pragma once

#include <deque>
#include <mutex>
#include <string>

extern "C" void logger_task(void*);

/// Logger singleton
class Logger
{
public:
    static Logger& instance();

    void set_api_token(const std::string& token);
    
    void set_gateway_token(const std::string& token);
    
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
    
    void log_unknown_card(const std::string& card_id);

private:
    Logger() = default;

    ~Logger() = default;

    void thread_body();
    
    struct Item {
        enum class Type {
            Debug,
            Backend,
            Unknown_card
        };
        static const int MAX_SIZE = 200; // Max length of debug messages
        static const int STAMP_SIZE = 19; // YYYY-mm-dd HH:MM:SS
        Type type = Type::Debug;
        int user_id = 0;
        char stamp[STAMP_SIZE+1];
        char text[MAX_SIZE+1];
    };
    std::deque<Item> q;
    std::mutex mutex;
    std::string gw_token;
    std::string api_token;
    bool verbose = false;
    bool log_to_gateway = false;

    friend void logger_task(void*);
};

// Needed in main(), so we just use a global function
extern void fatal_error(const std::string& msg);
