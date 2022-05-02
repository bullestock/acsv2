#pragma once

#include "util.h"

#include <RDM6300.h>

#include <map>

/// Cache for card info. Retrieves card info from panopticon and caches it.
class Card_cache
{
public:
    using Card_id = RDM6300::Card_id;
    
    Card_cache();

    bool has_access(Card_id id);

    bool has_access(const std::string& id);

private:
    using Cache = std::map<Card_id, util::time_point>;
    Cache cache;
    std::string api_token;
};
