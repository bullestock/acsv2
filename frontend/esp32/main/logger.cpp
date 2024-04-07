#include "logger.h"

#include "defs.h"
#include "nvs.h"
#include "http.h"
#include "util.h"

#include "cJSON.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"

#include <string.h>

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

time_t Logger::make_timestamp(char* stamp)
{
    time_t current = 0;
    time(&current);
    make_timestamp(current, stamp);
    return current;
}

void Logger::make_timestamp(time_t t, char* stamp)
{
    struct tm timeinfo;
    gmtime_r(&t, &timeinfo);
    strftime(stamp, TIMESTAMP_SIZE, "%Y-%m-%d %H:%M:%S", &timeinfo);
}

void Logger::log(const std::string& s)
{
    char stamp[Logger::TIMESTAMP_SIZE];
    make_timestamp(stamp);

    if (!log_to_gateway)
    {
        printf("%s %s\n", stamp, s.c_str());
        return;
    }
    Item item{ Item::Type::Debug };
    strncpy(item.stamp, stamp, std::min<size_t>(Item::STAMP_SIZE, strlen(stamp)));
    strncpy(item.text, s.c_str(), std::min<size_t>(Item::MAX_SIZE, s.size()));
    std::lock_guard<std::mutex> g(mutex);
    if (q.size() > 20)
    {
        ESP_LOGE(TAG, "Logger: Queue overflow");
        ++nof_overflows;
        return;
    }
    q.push_front(item);
}

void Logger::log_verbose(const std::string& s)
{
    if (verbose)
        log(s);
}

void Logger::log_backend(int user_id, const std::string& s)
{
    char stamp[Logger::TIMESTAMP_SIZE];
    make_timestamp(stamp);

    if (!log_to_gateway)
    {
        printf("%s %s\n", stamp, s.c_str());
        return;
    }
    Item item{ Item::Type::Backend, user_id };
    strncpy(item.stamp, stamp, std::min<size_t>(Item::STAMP_SIZE, strlen(stamp)));
    strncpy(item.text, s.c_str(), std::min<size_t>(Item::MAX_SIZE, s.size()));
    std::lock_guard<std::mutex> g(mutex);
    if (q.size() > 100)
    {
        ESP_LOGE(TAG, "Logger: Queue overflow");
        return;
    }
    q.push_front(item);
}

void Logger::log_unknown_card(Card_id card_id)
{
    Item item{ Item::Type::Unknown_card };
    sprintf(item.text, CARD_ID_FORMAT, card_id);
    std::lock_guard<std::mutex> g(mutex);
    if (q.size() > 5)
    {
        ESP_LOGE(TAG, "Logger: unknown_cards: Queue overflow");
        return;
    }
    q.push_front(item);
}

void Logger::log_sync_start()
{
    esp_http_client_config_t config {
        .host = "acsgateway.hal9k.dk",
        .path = "/acslog",
        .cert_pem = howsmyssl_com_root_cert_pem_start,
        .event_handler = http_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
    };
    debug_client = esp_http_client_init(&config);
    esp_http_client_set_method(debug_client, HTTP_METHOD_POST);
}

bool Logger::log_sync_do(const char* stamp, const char* text)
{
    auto payload = cJSON_CreateObject();
    cJSON_wrapper jw(payload);
    auto jtoken = cJSON_CreateString(gw_token.c_str());
    cJSON_AddItemToObject(payload, "token", jtoken);
    auto jstamp = cJSON_CreateString(stamp);
    cJSON_AddItemToObject(payload, "timestamp", jstamp);
    auto jtext = cJSON_CreateString(text);
    cJSON_AddItemToObject(payload, "text", jtext);
    auto jidentifier = cJSON_CreateString(get_identifier().c_str());
    cJSON_AddItemToObject(payload, "device", jidentifier);

    char* data = cJSON_Print(payload);
    if (!data)
    {
        ESP_LOGE(TAG, "Logger: cJSON_Print() returned nullptr");
        return false;
    }
    cJSON_Print_wrapper pw(data);
    esp_http_client_set_post_field(debug_client, data, strlen(data));

    esp_http_client_set_header(debug_client, "Content-Type", "application/json");
    int attempt = 0;
    while (attempt < 3)
    {
        esp_err_t err = esp_http_client_perform(debug_client);
        
        if (err == ESP_OK)
        {
            int code = esp_http_client_get_status_code(debug_client);
            if (code == 200)
                return true;
            ESP_LOGI(TAG, "acslog: HTTP %d", code);
        }
        else
            ESP_LOGE(TAG, "acslog: error %s", esp_err_to_name(err));
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    return false;
}

void Logger::log_sync_end()
{
    esp_http_client_close(debug_client);
    esp_http_client_cleanup(debug_client);
}

void Logger::thread_body()
{
    Item item;
    while (1)
    {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
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
                    if (gw_token.empty())
                        break;
                    if (state != State::debug)
                    {
                        log_sync_start();
                        state = State::debug;
                    }
                    log_sync_do(item.stamp, item.text);
                }
                break;

            case Item::Type::Backend:
                {
                    if (api_token.empty())
                        break;
                    if (state == State::debug)
                    {
                        log_sync_end();
                        state = State::backend;
                    }

                    esp_http_client_config_t config {
                        .host = "panopticon.hal9k.dk",
                        .path = "/api/v1/logs",
                        .cert_pem = howsmyssl_com_root_cert_pem_start,
                        .event_handler = http_event_handler,
                        .transport_type = HTTP_TRANSPORT_OVER_SSL,
                    };
                    esp_http_client_handle_t client = esp_http_client_init(&config);
                    Http_client_wrapper w(client);

                    esp_http_client_set_method(client, HTTP_METHOD_POST);
                    auto payload = cJSON_CreateObject();
                    cJSON_wrapper jw(payload);
                    auto jtoken = cJSON_CreateString(api_token.c_str());
                    cJSON_AddItemToObject(payload, "api_token", jtoken);
                    auto log = cJSON_CreateObject();
                    auto user = cJSON_CreateNumber(item.user_id);
                    cJSON_AddItemToObject(log, "user_id", user);
                    auto text = cJSON_CreateString(item.text);
                    cJSON_AddItemToObject(log, "message", text);
                    cJSON_AddItemToObject(payload, "log", log);

                    char* data = cJSON_Print(payload);
                    if (!data)
                    {
                        ESP_LOGE(TAG, "Logger: cJSON_Print() returned nullptr");
                        break;
                    }
                    cJSON_Print_wrapper pw(data);
                    esp_http_client_set_post_field(client, data, strlen(data));

                    const char* content_type = "application/json";
                    esp_http_client_set_header(client, "Content-Type", content_type);
                    esp_err_t err = esp_http_client_perform(client);

                    if (err == ESP_OK)
                        ESP_LOGI(TAG, "logs: HTTP %d", esp_http_client_get_status_code(client));
                    else
                        ESP_LOGE(TAG, "logs: error %s", esp_err_to_name(err));
                }
                break;

            case Item::Type::Unknown_card:
                {
                    if (api_token.empty())
                        break;

                    if (state == State::debug)
                    {
                        log_sync_end();
                        state = State::unknown;
                    }
                    esp_http_client_config_t config {
                        .host = "panopticon.hal9k.dk",
                        .path = "/api/v1/unknown_cards",
                        .cert_pem = howsmyssl_com_root_cert_pem_start,
                        .event_handler = http_event_handler,
                        .transport_type = HTTP_TRANSPORT_OVER_SSL,
                    };
                    esp_http_client_handle_t client = esp_http_client_init(&config);
                    Http_client_wrapper w(client);

                    esp_http_client_set_method(client, HTTP_METHOD_POST);
                    auto payload = cJSON_CreateObject();
                    cJSON_wrapper jw(payload);
                    auto jtoken = cJSON_CreateString(api_token.c_str());
                    cJSON_AddItemToObject(payload, "api_token", jtoken);
                    auto text = cJSON_CreateString(item.text);
                    cJSON_AddItemToObject(payload, "card_id", text);

                    char* data = cJSON_Print(payload);
                    if (!data)
                    {
                        ESP_LOGE(TAG, "Logger: cJSON_Print() returned nullptr");
                        break;
                    }
                    cJSON_Print_wrapper pw(data);
                    esp_http_client_set_post_field(client, data, strlen(data));

                    const char* content_type = "application/json";
                    esp_http_client_set_header(client, "Content-Type", content_type);
                    esp_err_t err = esp_http_client_perform(client);

                    if (err == ESP_OK)
                        ESP_LOGI(TAG, "unknown_cards: HTTP %d", esp_http_client_get_status_code(client));
                    else
                        ESP_LOGE(TAG, "unknown_cards: error %s", esp_err_to_name(err));
                }
                break;
            }
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
