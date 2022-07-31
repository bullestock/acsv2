#include "logger.h"
#include "util.h"

#include <fmt/core.h>
#include <fmt/chrono.h>

#include <restclient-cpp/restclient.h>
#include <restclient-cpp/connection.h>

#include <fstream>
#include <iomanip>
#include <iostream>

constexpr auto DEBUG_URL = "https://acsgateway.hal9k.dk/acslog";
constexpr auto BACKEND_URL = "https://panopticon.hal9k.dk/api/v1/logs";

Logger& Logger::instance()
{
    static Logger the_instance;
    return the_instance;
}

Logger::Logger()
    : q(25),
      thread([this](){ thread_body(); })
{
    std::ifstream is("./gw-token");
    std::getline(is, gw_token);
    if (gw_token.empty())
    {
        std::cerr << "Missing gateway API token\n";
        exit(1);
    }
    is = std::ifstream("./api-token");
    std::getline(is, api_token);
    if (api_token.empty())
    {
        fatal_error("Missing API token");
        exit(1);
    }
}

Logger::~Logger()
{
    stop = true;
    if (thread.joinable())
        thread.join();
}

void Logger::log(const std::string& s)
{
    const auto stamp = fmt::format("{:%Y-%m-%d %H:%M:%S}", fmt::gmtime(util::now()));
    if (!log_to_gateway)
    {
        std::cout << fmt::format("{} {}", stamp, s) << std::endl;
        return;
    }
    Item item{ Item::Type::Debug };
    strncpy(item.stamp, stamp.c_str(), std::min<size_t>(Item::STAMP_SIZE, stamp.size()));
    strncpy(item.text, s.c_str(), std::min<size_t>(Item::MAX_SIZE, s.size()));
    q.push(item);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void Logger::log_verbose(const std::string& s)
{
    if (verbose)
        log(s);
}

void Logger::log_backend(int user_id, const std::string& s)
{
    const auto stamp = fmt::format("{:%Y-%m-%d %H:%M:%S}", fmt::gmtime(util::now()));
    if (!log_to_gateway)
    {
        std::cout << fmt::format("{} {}", stamp, s) << std::endl;
        return;
    }
    Item item{ Item::Type::Backend, user_id };
    strncpy(item.stamp, stamp.c_str(), std::min<size_t>(Item::STAMP_SIZE, stamp.size()));
    strncpy(item.text, s.c_str(), std::min<size_t>(Item::MAX_SIZE, s.size()));
    q.push(item);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void Logger::thread_body()
{
    Item item;
    while (!stop)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        try
        {
            if (q.empty() || !q.pop(item))
                continue;

            switch (item.type)
            {
            case Item::Type::Debug:
            {
                RestClient::Connection conn(DEBUG_URL);
                conn.AppendHeader("Content-Type", "application/json");
                util::json payload;
                payload["token"] = gw_token;
                payload["timestamp"] = item.stamp;
                payload["text"] = item.text;
                const auto resp = conn.post("", payload.dump());
                if (resp.code != 200)
                {
                    std::cout << "Gateway log error code " << resp.code << std::endl;
                    std::cout << "- body: " << resp.body << std::endl;
                }
            }
            break;

            case Item::Type::Backend:
            {
                RestClient::Connection conn(BACKEND_URL);
                conn.AppendHeader("Content-Type", "application/json");
                util::json payload;
                payload["api_token"] = api_token;
                util::json log;
                log["user_id"] = item.user_id;
                log["message"] = item.text;
                payload["log"] = log;
                const auto resp = conn.post("", payload.dump());
                if (resp.code != 200)
                {
                    std::cout << "Backend log error code " << resp.code << std::endl;
                    std::cout << "- body: " << resp.body << std::endl;
                }
            }
            break;
            }
        }
        catch (const std::exception& e)
        {
            std::cout << "Exception in logger: " << e.what() << std::endl;
        }
    }
}
