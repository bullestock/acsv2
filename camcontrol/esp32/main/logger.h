#pragma once

#include <deque>
#include <mutex>
#include <string>

#include "esp_http_client.h"

extern "C" void logger_task(void*);

/// Logger singleton
class Logger
{
public:
    static constexpr int TIMESTAMP_SIZE = 26;
    
    static Logger& instance();

    void set_token(const std::string& token);
    
    void set_verbose(bool on)
    {
        verbose = on;
    }

    void set_log_to_gateway(bool on)
    {
        log_to_gateway = on;
    }

    int get_nof_overflows() const
    {
        return nof_overflows;
    }

    /// Log to console and gateway.
    void log(const std::string&);

    /// If 'verbose' is set, log to console and gateway.
    void log_verbose(const std::string&);

    /// Make a timestamp string. Buffer must be TIMESTAMP_SIZE bytes.
    static time_t make_timestamp(char* stamp);

    static void make_timestamp(time_t t, char* stamp);

    /// Internal function, but also used by core dump upload because reasons
    void log_sync_start();

    /// Internal function, but also used by core dump upload because reasons
    bool log_sync_do(const char* stamp, const char* text);

    /// Internal function, but also used by core dump upload because reasons
    void log_sync_end();
    
private:
    Logger() = default;

    ~Logger() = default;

    void thread_body();

    struct Item {
        static const int MAX_SIZE = 200; // Max length of debug messages
        static const int STAMP_SIZE = 19; // YYYY-mm-dd HH:MM:SS
        int user_id = 0;
        char stamp[STAMP_SIZE+1];
        char text[MAX_SIZE+1];
    };

    std::deque<Item> q;
    std::mutex mutex;
    std::string gw_token;
    bool verbose = false;
    bool log_to_gateway = false;
    int nof_overflows = 0;
    esp_http_client_handle_t debug_client = nullptr;
    
    friend void logger_task(void*);
};

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
