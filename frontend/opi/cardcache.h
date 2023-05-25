#pragma once

#include "util.h"

#include <RDM6300.h>

#include <map>
#include <mutex>
#include <thread>

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
    };
    
    Card_cache();

    ~Card_cache();

    Result has_access(Card_id id);

    Result has_access(const std::string& id);

private:
    /// Updates cache in background
    void update_cache();

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
    std::thread cache_thread;
    bool stop = false;
};
