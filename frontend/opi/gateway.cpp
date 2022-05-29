#include "gateway.h"
#include "logger.h"
#include "util.h"

#include <restclient-cpp/restclient.h>
#include <restclient-cpp/connection.h>

#include <fstream>
#include <iomanip>
#include <iostream>

constexpr auto BASE_URL = "https://acsgateway.hal9k.dk/";

Gateway::Gateway()
    : thread([this](){ thread_body(); })
{
    std::ifstream is("./gw-token");
    std::getline(is, token);
    if (token.empty())
    {
        std::cerr << "Missing gateway API token\n";
        exit(1);
    }
}

Gateway::~Gateway()
{
    stop = true;
    thread.join();
}
    
void Gateway::set_status(const util::json& status)
{
    std::lock_guard<std::mutex> g(mutex);
    current_status = status;
}

std::string Gateway::get_action()
{
    std::lock_guard<std::mutex> g(mutex);
    return current_action;
}

void Gateway::thread_body()
{
    while (!stop)
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        util::json status;
        {
            std::lock_guard<std::mutex> g(mutex);
            status = current_status;
        }
        post_status(status);
        const auto action = do_get_action();
        {
            std::lock_guard<std::mutex> g(mutex);
            current_action = action;
        }
    }
}

void Gateway::post_status(const util::json& status)
{
    RestClient::Connection conn(BASE_URL);
    conn.AppendHeader("Content-Type", "application/json");
    conn.AppendHeader("Accept", "application/json");
    util::json payload;
    payload["token"] = token;
    payload["status"] = status;
    const auto resp = conn.post("acsstatus", payload.dump());
    if (resp.code != 200)
    {
        Logger::instance().log("Gateway error code " + resp.code);
        Logger::instance().log("- body: " + resp.body);
    }
}

std::string Gateway::do_get_action()
{
    try
    {
        RestClient::Connection conn(BASE_URL);
        conn.AppendHeader("Content-Type", "application/json");
        conn.AppendHeader("Accept", "application/json");
        util::json payload;
        payload["token"] = token;
        const auto resp = conn.post("acsquery", payload.dump());
        if (resp.code != 200)
        {
            Logger::instance().log("Gateway error code " + resp.code);
            Logger::instance().log("- body: " + resp.body);
            return "";
        }
        const auto json_resp = util::json::parse(resp.body);
        const auto action = json_resp.find("action");
        if (action == json_resp.end())
        {
            Logger::instance().log("No action in gateway reply");
            return "";
        }
        if (action->is_string())
            return action->get<std::string>();
    }
    catch (const std::exception& e)
    {
        Logger::instance().log(fmt::format("GW exception: {}", e.what()));
    }
    return "";
}
