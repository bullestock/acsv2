#include "gateway.h"

#include "defs.h"
#include "display.h"
#include "format.h"
#include "http.h"
#include "nvs.h"
#include "slack.h"
#include "util.h"

#include "cJSON.h"

#include <memory>
#include <string>
#include <string.h>
#include <stdlib.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mbedtls/base64.h>
#include <esp_app_desc.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_tls.h>
#include <nvs_flash.h>

static constexpr const char* TAG = "gw";

Gateway& Gateway::instance()
{
    static Gateway the_instance;
    return the_instance;
}

void Gateway::set_token(const std::string& _token)
{
    token = _token;
}

std::pair<std::string, std::string> Gateway::get_and_clear_action()
{
    std::lock_guard<std::mutex> g(mutex);
    const auto result = std::make_pair(current_action, current_action_arg);
    current_action.clear();
    current_action_arg.clear();
    return result;
}

bool Gateway::get_allow_open() const
{
    return allow_open;
}

void Gateway::check_action(esp_http_client_handle_t client, const char* data, char* buffer)
{
    esp_err_t err = esp_http_client_perform(client);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "check_action: error %s", esp_err_to_name(err));
        ESP_LOGE(TAG, "Memory %zu", heap_caps_get_free_size(MALLOC_CAP_8BIT));
        return;
    }

    const int code = esp_http_client_get_status_code(client);
    if (code != 200)
    {
        ESP_LOGE(TAG, "check_action: HTTP %d", code);
        return;
    }

    auto root = cJSON_Parse(buffer);
    cJSON_wrapper jwr(root);
    if (root)
    {
        auto action_node = cJSON_GetObjectItem(root, "action");
        if (action_node && action_node->type == cJSON_String)
        {
            std::lock_guard<std::mutex> g(mutex);
            current_action = action_node->valuestring;
            ESP_LOGI(TAG, "action = %s", current_action.c_str());
            auto arg_node = cJSON_GetObjectItem(root, "arg");
            if (arg_node && arg_node->type == cJSON_String)
                current_action_arg = arg_node->valuestring;
        }
        auto allow_open_node = cJSON_GetObjectItem(root, "allow_open");
        if (allow_open_node && cJSON_IsBool(allow_open_node))
            allow_open = cJSON_IsTrue(allow_open_node);
    }
}

void Gateway::thread_body()
{
    auto payload = cJSON_CreateObject();
    cJSON_wrapper jw(payload);
    auto jtoken = cJSON_CreateString(token.c_str());
    cJSON_AddItemToObject(payload, "token", jtoken);
    auto jidentifier = cJSON_CreateString(get_identifier().c_str());
    cJSON_AddItemToObject(payload, "device", jidentifier);

    const char* data = cJSON_Print(payload);
    if (!data)
    {
        ESP_LOGE(TAG, "cJSON_Print() returned nullptr");
        vTaskDelete(nullptr);
        return;
    }

    constexpr int HTTP_MAX_OUTPUT = 255;
    char buffer[HTTP_MAX_OUTPUT+1];
    Http_data http_data;
    http_data.buffer = buffer;
    http_data.max_output = HTTP_MAX_OUTPUT;
    esp_http_client_config_t config {
        .host = "acsgateway.hal9k.dk",
        .path = "/acsquery",
        .event_handler = http_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .user_data = &http_data,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGE(TAG, "client_init failed");
        vTaskDelete(nullptr);
        return;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, data, strlen(data));

    const char* content_type = "application/json";
    esp_http_client_set_header(client, "Content-Type", content_type);

    while (1)
    {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        const auto free1 = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        http_data.output_len = 0;
        check_action(client, data, buffer);
        ESP_LOGI(TAG, "Memory delta %d", static_cast<int>(heap_caps_get_free_size(MALLOC_CAP_8BIT) - free1));
    }
}

void Gateway::set_action(const std::string& action, const std::string& action_arg)
{
    std::lock_guard<std::mutex> g(mutex);
    current_action = action;
    current_action_arg = action_arg;
    ESP_LOGI(TAG, "action = %s", current_action.c_str());
}

void gw_task(void*)
{
    Gateway::instance().thread_body();
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
