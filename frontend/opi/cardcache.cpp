#include "cardcache.h"

constexpr util::duration MAX_CACHE_AGE = std::chrono::minutes(15);

Card_cache::Card_cache()
{
}

bool Card_cache::has_access(Card_cache::Card_id id)
{
    const auto it = cache.find(id);
    if (it != cache.end())
    {
        if (util::now() - it->second < MAX_CACHE_AGE)
            return true;
        // Cache entry is outdated
    }
    return false;
}
