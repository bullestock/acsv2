#pragma once

#include <RDM6300.h>

#include <deque>
#include <mutex>
#include <string>

extern "C" void logger_task(void*);

/// Logger singleton
class Logger
{
public:
    using Card_id = RDM6300::Card_id;

    static constexpr int TIMESTAMP_SIZE = 26;
    
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

    /// Log to console and gateway.
    void log(const std::string&);

    /// If 'verbose' is set, log to console and gateway.
    void log_verbose(const std::string&);

    /// Log to console and panopticon.
    void log_backend(int user_id, const std::string&);
    
    /// Log to console and panopticon.
    void log_unknown_card(Card_id card_id);

    /// Make a timestamp string. Buffer must be TIMESTAMP_SIZE bytes.
    static time_t make_timestamp(char* stamp);

    /// Log synchronously to gateway.
    void log_sync(const char* stamp, const char* text);

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

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
