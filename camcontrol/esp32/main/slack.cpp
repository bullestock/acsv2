#include "slack.h"

#include "defs.h"
#include "format.h"
#include "http.h"
#include "util.h"

#include <cJSON.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_tls.h"
      
Slack_writer& Slack_writer::instance()
{
    static Slack_writer the_instance;
    return the_instance;
}

void Slack_writer::set_token(const std::string& token)
{
    api_token = token;
}

void Slack_writer::set_params(bool test_mode)
{
    is_test_mode = test_mode;
}

void Slack_writer::set_status(const std::string& status, bool include_general)
{
    if (status != last_status)
    {
        send_message(status, { .general = include_general });
        last_status = status;
    }
}

void Slack_writer::send_message(const std::string& message, Channels channels)
{
    send_to_channel(is_test_mode ? "testing" : "private-monitoring", message);
    if (channels.general && !is_test_mode)
        send_to_channel("general", message);
    if (channels.info && !is_test_mode)
        send_to_channel("jeg-står-herude-og-banker-på", message);
}

void Slack_writer::send_to_channel(const std::string& channel,
                                   const std::string& message)
{
    ESP_LOGI(TAG, "Slack: #%s: %s", channel.c_str(), message.c_str());
    if (api_token.empty())
    {
        ESP_LOGE(TAG, "Slack: No API token");
        return;
    }
    Item item{ channel, message };
    std::lock_guard<std::mutex> g(mutex);
    if (q.size() > 100)
    {
        ESP_LOGE(TAG, "Slack: Queue overflow");
        return;
    }
    q.push_back(item);
}

void Slack_writer::thread_body()
{
    Item item;
    while (1)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);

        {
            std::lock_guard<std::mutex> g(mutex);
            if (q.empty())
                continue;
            item = q.back();
            q.pop_back();
        }

        if (api_token.empty())
        {
            ESP_LOGE(TAG, "Slack: Missing credentials");
            continue;
        }

        std::lock_guard<std::mutex> g(http_mutex);

        esp_http_client_config_t config {
            .host = "slack.com",
            .path = "/api/chat.postMessage",
            .event_handler = http_event_handler,
            .transport_type = HTTP_TRANSPORT_OVER_SSL,
            .crt_bundle_attach = esp_crt_bundle_attach,
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);
        Http_client_wrapper w(client);
        esp_http_client_set_method(client, HTTP_METHOD_POST);

        while (1)
        {
            do_post(client, item);
        
            {
                std::lock_guard<std::mutex> g(mutex);
                if (q.empty())
                    break;
                item = q.back();
                q.pop_back();
            }
        }
    }
}

void Slack_writer::do_post(esp_http_client_handle_t client, const Item& item)
{
    ESP_LOGI(TAG, "Slack: do_post(%s)", item.message.c_str());
    
    auto payload = cJSON_CreateObject();
    cJSON_wrapper jw(payload);
    auto jchannel = cJSON_CreateString(item.channel.c_str());
    cJSON_AddItemToObject(payload, "channel", jchannel);
    auto emoji = cJSON_CreateString(":panopticon:");
    cJSON_AddItemToObject(payload, "icon_emoji", emoji);
    auto full = cJSON_CreateString("full");
    cJSON_AddItemToObject(payload, "parse", full);
    auto text = cJSON_CreateString(item.message.c_str());
    cJSON_AddItemToObject(payload, "text", text);

    char* data = cJSON_Print(payload);
    if (!data)
    {
        ESP_LOGE(TAG, "Slack: cJSON_Print() returned nullptr");
        return;
    }
    cJSON_Print_wrapper pw(data);
    esp_http_client_set_post_field(client, data, strlen(data));

    esp_http_client_set_header(client, "Content-Type", "application/json");
    const auto auth = std::string("Bearer ") + api_token;
    esp_http_client_set_header(client, "Authorization", auth.c_str());
    const esp_err_t err = esp_http_client_perform(client);

    if (err != ESP_OK)
        ESP_LOGE(TAG, "Slack: error %s", esp_err_to_name(err));
}

void slack_task(void*)
{
    Slack_writer::instance().thread_body();
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
