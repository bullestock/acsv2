#include "cardcache.h"
#include "logger.h"

#include <fstream>
#include <iomanip>
#include <iostream>

#include <fmt/core.h>

#include <restclient-cpp/restclient.h>
#include <restclient-cpp/connection.h>

constexpr util::duration MAX_CACHE_AGE = std::chrono::minutes(15);

constexpr auto BASE_URL = "https://panopticon.hal9k.dk/api";

Card_cache::Card_cache()
    : cache_thread([this](){ update_cache(); })
{
    RestClient::init();
    std::ifstream is("./api-token");
    std::getline(is, api_token);
    if (api_token.empty())
    {
        fatal_error("Missing API token");
        exit(1);
    }
}

Card_cache::~Card_cache()
{
    stop = true;
    if (cache_thread.joinable())
        cache_thread.join();
}

Card_cache::Result Card_cache::has_access(Card_cache::Card_id id)
{
    std::lock_guard<std::mutex> g(cache_mutex);
    const auto it = cache.find(id);
    if (it != cache.end())
    {
        if (util::now() - it->second.last_update < MAX_CACHE_AGE)
        {
            Logger::instance().log(fmt::format("{:010X}: cached", id));
            Logger::instance().log_backend(it->second.user_id, "Granted entry");
            return Result(Access::Allowed, it->second.user_int_id);
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
    const auto resp = conn.post("/v1/permissions", payload.dump());
    Logger::instance().log(fmt::format("resp.code: {}", resp.code));
    Logger::instance().log(fmt::format("resp.body: {}", resp.body));
    if (resp.code != 200)
        return Result(resp.code == 404 ? Access::Unknown : Access::Error, -1);
    const auto resp_body = util::json::parse(resp.body);
    if (resp_body.is_null())
        return Result(Access::Error, -1);
    const auto allowed = resp_body.find("allowed");
    if (allowed == resp_body.end() ||
        !allowed->is_boolean())
        return Result(Access::Error, -1);
    int user_int_id = -1;
    const auto res = allowed->get<bool>();
    if (res)
    {
        const int user_id = resp_body["id"];
        user_int_id = resp_body["int_id"];
        cache[id] = { user_id, user_int_id, util::now() };
        Logger::instance().log_backend(user_id, "Granted entry");
    }
    return Result(res ? Access::Allowed : Access::Forbidden, user_int_id);
}

Card_cache::Card_id get_id_from_string(const std::string& s)
{
    std::istringstream is(s);
    Card_cache::Card_id id = 0;
    is >> std::hex >> id;
    return id;
}

Card_cache::Result Card_cache::has_access(const std::string& sid)
{
    return has_access(get_id_from_string(sid));
}

void Card_cache::update_cache()
{
    while (!stop)
    {
        std::this_thread::sleep_for(std::chrono::minutes(1));
        try
        {
            // Fetch card info
            RestClient::Connection conn(BASE_URL);
            conn.SetTimeout(5);
            conn.AppendHeader("Content-Type", "application/json");
            conn.AppendHeader("Accept", "application/json");
            conn.AppendHeader("Authorization", "Token "+api_token);
            const auto resp = conn.get("/v2/permissions/");
            Logger::instance().log_verbose(fmt::format("resp.code: {}", resp.code));
            Logger::instance().log_verbose(fmt::format("resp.body: {}", resp.body));
            if (resp.code != 200)
            {
                Logger::instance().log(fmt::format("Error: Unexpected response from /v2/permissions: {}", resp.code));
                continue;
            }
            const auto resp_body = util::json::parse(resp.body);
            if (!resp_body.is_array())
            {
                Logger::instance().log("Error: Response from /v2/permissions is not an array");
                continue;
            }
            // Create new cache
            Cache new_cache;
            for (const auto& e : resp_body)
                new_cache[get_id_from_string(e["card_id"])] = { e["id"], e["int_id"], util::now() };
            {
                // Store
                std::lock_guard<std::mutex> g(cache_mutex);
                cache.swap(new_cache);
            }
            Logger::instance().log("Card cache updated");
        }
        catch (const std::exception& e)
        {
            Logger::instance().log(fmt::format("Exception updating card cache: {}", e.what()));
        }
    }
}
