#include "cardcache.h"

#include <iomanip>
#include <iostream>

#include <fmt/core.h>

#include <restclient-cpp/restclient.h>
#include <restclient-cpp/connection.h>

constexpr util::duration MAX_CACHE_AGE = std::chrono::minutes(15);

constexpr auto BASE_URL = "https://panopticon.hal9k.dk/api/v1";
constexpr auto TOKEN = "topsecret";

Card_cache::Card_cache()
{
    RestClient::init();
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

    auto conn = std::make_unique<RestClient::Connection>(BASE_URL);
    conn->SetTimeout(5);
    conn->AppendHeader("Content-Type", "application/json");
    conn->AppendHeader("Accept", "application/json");
    util::json payload;
    payload["api_token"] = TOKEN;
    payload["card_id"] = fmt::format("{:10X}", id);
    std::cout << "POST: " << payload << std::endl;
    const auto resp = conn->post("/permissions", payload.dump());
    std::cout << "resp.code: " << resp.code << std::endl;
    std::cout << "resp.body: " << resp.body << std::endl;

    return false;
}
