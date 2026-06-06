#pragma once

#include "RDM6300.h"

#include <deque>
#include <mutex>
#include <string>

#include "esp_http_client.h"

extern "C" void logger_task(void*);

/// Logger singleton
class Logger
{
public:
    using Card_id = RDM6300::Card_id;

    static Logger& instance();

    void set_api_token(const std::string& token);
    
    void set_gateway_token(const std::string& token);
    
    /// Log to console and panopticon.
    void log_backend(int user_id, const std::string&);
    
    /// Log to console and panopticon.
    void log_unknown_card(Card_id card_id);

private:
    Logger() = default;

    ~Logger() = default;

    void thread_body();

    struct Item {
        enum class Type {
            Backend,
            Unknown_card
        };
        static const int MAX_SIZE = 200; // Max length of messages
        static const int STAMP_SIZE = 19; // YYYY-mm-dd HH:MM:SS
        Type type = Type::Backend;
        int user_id = 0;
        char stamp[STAMP_SIZE+1];
        char text[MAX_SIZE+1];
    };

    enum class State
    {
        init,
        backend,
        unknown
    };
    State state = State::init;
    std::deque<Item> q;
    std::mutex mutex;
    std::string gw_token;
    std::string api_token;
    esp_http_client_handle_t debug_client = nullptr;
    
    friend void logger_task(void*);
};

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
