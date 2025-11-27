#include "logger.h"

#include "defs.h"
#include "mqtt.h"
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
#if USE_MQTT
    log_mqtt(s);
#else
    char stamp[Logger::TIMESTAMP_SIZE];
    make_timestamp(stamp);

    if (!log_to_gateway)
    {
        printf("%s %s\n", stamp, s.c_str());
        return;
    }
    Item item;
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
#endif
}

void Logger::log_verbose(const std::string& s)
{
    if (verbose)
        log(s);
}

void Logger::log_sync_start()
{
    esp_http_client_config_t config {
        .host = "acsgateway.hal9k.dk",
        .path = "/acslog",
        .event_handler = http_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
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
    auto jidentifier = cJSON_CreateString("camcontrol");
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
    bool init = false;
    while (1)
    {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        while (1)
        {
            vTaskDelay(10 / portTICK_PERIOD_MS);

            if (q.empty())
                continue;
            const auto item = q.back();
            q.pop_back();

            if (gw_token.empty())
                break;
            if (!init)
            {
                log_sync_start();
                init = true;
            }
            log_sync_do(item.stamp, item.text);
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
