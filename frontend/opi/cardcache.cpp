#include "cardcache.h"
#include "logger.h"

#include <fstream>
#include <iomanip>
#include <iostream>

#include <fmt/core.h>

#include <restclient-cpp/restclient.h>
#include <restclient-cpp/connection.h>

constexpr util::duration MAX_CACHE_AGE = std::chrono::minutes(15);

constexpr auto BASE_URL = "https://panopticon.hal9k.dk/api/v1";

Card_cache::Card_cache()
{
    RestClient::init();
    std::ifstream is("./api-token");
    std::getline(is, api_token);
    if (api_token.empty())
    {
        Logger::instance().fatal_error("Missing API token");
        exit(1);
    }
}

bool Card_cache::has_access(Card_cache::Card_id id)
{
    const auto it = cache.find(id);
    if (it != cache.end())
    {
        if (util::now() - it->second < MAX_CACHE_AGE)
        {
            Logger::instance().log(fmt::format("{:10X}: cached", id));
            return true;
        }
        Logger::instance().log(fmt::format("{:10X}: stale", id));
        // Cache entry is outdated
    }

    RestClient::Connection conn(BASE_URL);
    conn.SetTimeout(5);
    conn.AppendHeader("Content-Type", "application/json");
    conn.AppendHeader("Accept", "application/json");
    util::json payload;
    payload["api_token"] = api_token;
    payload["card_id"] = fmt::format("{:10X}", id);
    Logger::instance().log(fmt::format("POST: {}", payload));
    const auto resp = conn.post("/permissions", payload.dump());
    Logger::instance().log("resp.code: " + resp.code);
    Logger::instance().log("resp.body: " + resp.body);
    if (resp.code != 200)
        return false;
    const auto resp_body = util::json::parse(resp.body);
    if (resp_body.is_null())
        return false;
    const auto allowed = resp_body.find("allowed");
    if (allowed == resp_body.end() ||
        !allowed->is_boolean())
        return false;
    const auto res = allowed->get<bool>();
    cache[id] = util::now();
    return res;
}

bool Card_cache::has_access(const std::string& sid)
{
    std::istringstream is(sid);
    Card_id id = 0;
    is >> std::hex >> id;
    return has_access(id);
}
