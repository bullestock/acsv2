#include "logger.h"
#include "util.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

constexpr auto DEBUG_URL = "https://acsgateway.hal9k.dk/acslog";
constexpr auto LOG_URL =          "https://panopticon.hal9k.dk/api/v1/logs";
constexpr auto UNKNOWN_CARD_URL = "https://panopticon.hal9k.dk/api/v1/unknown_cards";

Logger& Logger::instance()
{
    static Logger the_instance;
    return the_instance;
}

Logger::Logger()
    : q(25)
{
}

void Logger::set_api_token(const std::string& token)
{
    api_token = token;
}

void Logger::set_gateway_token(const std::string& token)
{
    gw_token = token;
}

void Logger::log(const std::string& s)
{
    const std::string stamp = ""; //!!fmt::format("{:%Y-%m-%d %H:%M:%S}", fmt::gmtime(util::now()));
    if (!log_to_gateway)
    {
        printf("%s %s\n", stamp.c_str(), s.c_str());
        return;
    }
    Item item{ Item::Type::Debug };
    strncpy(item.stamp, stamp.c_str(), std::min<size_t>(Item::STAMP_SIZE, stamp.size()));
    strncpy(item.text, s.c_str(), std::min<size_t>(Item::MAX_SIZE, s.size()));
    q.push_front(item);
    vTaskDelay(10 / portTICK_PERIOD_MS);
}

void Logger::log_verbose(const std::string& s)
{
    if (verbose)
        log(s);
}

void Logger::log_backend(int user_id, const std::string& s)
{
    const std::string stamp = ""; //!! fmt::format("{:%Y-%m-%d %H:%M:%S}", fmt::gmtime(util::now()));
    if (!log_to_gateway)
    {
        printf("%s %s\n", stamp.c_str(), s.c_str());
        return;
    }
    Item item{ Item::Type::Backend, user_id };
    strncpy(item.stamp, stamp.c_str(), std::min<size_t>(Item::STAMP_SIZE, stamp.size()));
    strncpy(item.text, s.c_str(), std::min<size_t>(Item::MAX_SIZE, s.size()));
    q.push_front(item);
    vTaskDelay(10 / portTICK_PERIOD_MS);
}

void Logger::log_unknown_card(const std::string& card_id)
{
    Item item{ Item::Type::Unknown_card };
    strncpy(item.text, card_id.c_str(), std::min<size_t>(Item::MAX_SIZE, card_id.size()));
    q.push_front(item);
    vTaskDelay(10 / portTICK_PERIOD_MS);
}

void Logger::thread_body()
{
    Item item;
    while (1)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);

        if (q.empty())
            continue;
        item = q.back();
        q.pop_back();

        switch (item.type)
        {
        case Item::Type::Debug:
            {
                /*
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
                */
            }
            break;

        case Item::Type::Backend:
            {
                /*
                  RestClient::Connection conn(LOG_URL);
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
                */
            }
            break;

        case Item::Type::Unknown_card:
            {
                /*
                  RestClient::Connection conn(UNKNOWN_CARD_URL);
                  conn.AppendHeader("Content-Type", "application/json");
                  util::json payload;
                  payload["api_token"] = api_token;
                  payload["card_id"] = item.text;
                  const auto resp = conn.post("", payload.dump());
                  if (resp.code != 200)
                  {
                  std::cout << "Unknown card error code " << resp.code << std::endl;
                  std::cout << "- body: " << resp.body << std::endl;
                  }
                */
            }
            break;
        }
    }
}

void logger_task(void*)
{
    Logger::instance().thread_body();
}
