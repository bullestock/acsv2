#include "slack.h"

#include "defs.h"
#include "format.h"
#include "http.h"

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

void Slack_writer::announce_open()
{
    set_status(":tada: The space is now open!", true);
}

void Slack_writer::announce_closed()
{
    set_status(":sad_panda2: The space is no longer open", true);
}

void Slack_writer::set_status(const std::string& status, bool include_general)
{
    if (status != last_status)
    {
        send_message(status, include_general);
        last_status = status;
    }
}

void Slack_writer::send_message(const std::string& message, bool include_general)
{
    send_to_channel(is_test_mode ? "testing" : "private-monitoring", message);
    if (include_general && !is_test_mode)
        send_to_channel("general", message);
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

        if (q.empty())
            continue;
        item = q.back();
        q.pop_back();

        if (api_token.empty())
        {
            ESP_LOGE(TAG, "Slack: Missing credentials");
            continue;
        }

        esp_http_client_config_t config {
            .host = "slack.com",
            .path = "/api/chat.postMessage",
            .cert_pem = howsmyssl_com_root_cert_pem_start,
            .event_handler = http_event_handler,
            .transport_type = HTTP_TRANSPORT_OVER_SSL,
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);
        Http_client_wrapper w(client);

        esp_http_client_set_method(client, HTTP_METHOD_POST);
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

        const char* content_type = "application/json";
        esp_http_client_set_header(client, "Content-Type", content_type);
        const auto auth = std::string("Bearer ") + api_token;
        esp_http_client_set_header(client, "Authorization", auth.c_str());
        const esp_err_t err = esp_http_client_perform(client);

        if (err == ESP_OK)
            ESP_LOGI(TAG, "Slack: HTTP %d", esp_http_client_get_status_code(client));
        else
            ESP_LOGE(TAG, "Slack: error %s", esp_err_to_name(err));
    }
}

void slack_task(void*)
{
    Slack_writer::instance().thread_body();
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
