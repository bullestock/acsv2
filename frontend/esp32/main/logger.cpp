#include "defs.h"
#include "gateway.h"
#include "logger.h"
#include "util.h"

#include "cJSON.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"

#include <string.h>

extern const char howsmyssl_com_root_cert_pem_start[] asm("_binary_howsmyssl_com_root_cert_pem_start");
extern const char howsmyssl_com_root_cert_pem_end[]   asm("_binary_howsmyssl_com_root_cert_pem_end");

constexpr auto LOG_URL =          "https://panopticon.hal9k.dk/api/v1/logs";
constexpr auto UNKNOWN_CARD_URL = "https://panopticon.hal9k.dk/api/v1/unknown_cards";

Logger& Logger::instance()
{
    static Logger the_instance;
    return the_instance;
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
    time_t current = 0;
    time(&current);
    char stamp[26];
    struct tm timeinfo;
    gmtime_r(&current, &timeinfo);
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &timeinfo);

    if (!log_to_gateway)
    {
        printf("%s %s\n", stamp, s.c_str());
        return;
    }
    Item item{ Item::Type::Debug };
    strncpy(item.stamp, stamp, std::min<size_t>(Item::STAMP_SIZE, strlen(stamp)));
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
    time_t current = 0;
    time(&current);
    char stamp[26];
    struct tm timeinfo;
    gmtime_r(&current, &timeinfo);
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &timeinfo);

    if (!log_to_gateway)
    {
        printf("%s %s\n", stamp, s.c_str());
        return;
    }
    Item item{ Item::Type::Backend, user_id };
    strncpy(item.stamp, stamp, std::min<size_t>(Item::STAMP_SIZE, strlen(stamp)));
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

        ESP_LOGI(TAG, "Logger: Got item type %d", static_cast<int>(item.type));
        switch (item.type)
        {
        case Item::Type::Debug:
            {
                ESP_LOGI(TAG, "Logger: Debug %s", item.text);
                esp_http_client_config_t config {
                    .host = "acsgateway.hal9k.dk",
                    .path = "/acslog",
                    .cert_pem = howsmyssl_com_root_cert_pem_start,
                    .event_handler = http_event_handler,
                    .transport_type = HTTP_TRANSPORT_OVER_SSL,
                };
                esp_http_client_handle_t client = esp_http_client_init(&config);

                esp_http_client_set_method(client, HTTP_METHOD_POST);
                auto payload = cJSON_CreateObject();
                auto jtoken = cJSON_CreateString(gw_token.c_str());
                cJSON_AddItemToObject(payload, "token", jtoken);
                auto stamp = cJSON_CreateString(item.stamp);
                cJSON_AddItemToObject(payload, "timestamp", stamp);
                auto text = cJSON_CreateString(item.text);
                cJSON_AddItemToObject(payload, "text", text);

                const char* data = cJSON_Print(payload);
                if (!data)
                {
                    ESP_LOGI(TAG, "Logger: cJSON_Print() returned nullptr");
                    break;
                }
                esp_http_client_set_post_field(client, data, strlen(data));

                const char* content_type = "application/json";
                esp_http_client_set_header(client, "Content-Type", content_type);
                esp_err_t err = esp_http_client_perform(client);
                cJSON_Delete(payload);
                if (err == ESP_OK)
                    ESP_LOGI(TAG, "Logger GW status = %d", esp_http_client_get_status_code(client));
                else
                    ESP_LOGE(TAG, "Error performing http request %s", esp_err_to_name(err));

                esp_http_client_cleanup(client);
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

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
