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
    
    Card_cache();

    ~Card_cache();

    bool has_access(Card_id id);

    bool has_access(const std::string& id);

private:
    /// Updates cache in background
    void update_cache();
    
    using Cache = std::map<Card_id, util::time_point>;
    Cache cache;
    std::mutex cache_mutex;
    std::string api_token;
    std::thread cache_thread;
    bool stop = false;
};
