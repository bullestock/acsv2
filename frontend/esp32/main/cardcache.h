#pragma once

#include "util.h"

#include <RDM6300.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_http_client.h"

#include <map>
#include <mutex>

extern "C" void card_cache_task(void*);

/// Cache for card info. Retrieves card info from panopticon and caches it.
class Card_cache
{
public:
    using Card_id = RDM6300::Card_id;

    enum class Access
    {
        Error,
        Allowed,
        Forbidden,
        Unknown,
    };

    struct Result
    {
        Access access;
        int user_id; // internal ID
        std::string error_msg;
    };

    static Card_cache& instance();

    static Card_id get_id_from_string(const std::string& s);

    void set_api_token(const std::string& token);

    Result has_access(Card_id id);

private:
    Card_cache() = default;

    /// Updates cache in background
    void thread_body();

    Result get_result(esp_http_client_handle_t client, const char* buffer, int id);

    struct User_info
    {
        int user_id = 0;
        int user_int_id = 0;
        util::time_point last_update;
    };
    using Cache = std::map<Card_id, User_info>;
    Cache cache;
    std::mutex cache_mutex;
    std::string api_token;

    friend void card_cache_task(void*);
};

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
