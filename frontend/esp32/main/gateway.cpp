#include "gateway.h"

#include "defs.h"
#include "http.h"
#include "util.h"

#include "cJSON.h"

#include <memory>
#include <string>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "nvs_flash.h"

Gateway& Gateway::instance()
{
    static Gateway the_instance;
    return the_instance;
}

void Gateway::set_token(const std::string& _token)
{
    token = _token;
}

void Gateway::set_status(const cJSON* status)
{
    auto payload = cJSON_CreateObject();
    cJSON_wrapper jw(payload);
    auto jtoken = cJSON_CreateString(token.c_str());
    cJSON_AddItemToObject(payload, "token", jtoken);
    cJSON_AddItemReferenceToObject(payload, "status",
                                   const_cast<cJSON*>(status)); // alas
    std::lock_guard<std::mutex> g(mutex);
    auto data = cJSON_Print(payload);
    cJSON_Print_wrapper pw(data);
    current_status = data;
}

std::string Gateway::get_action()
{
    std::lock_guard<std::mutex> g(mutex);
    return current_action;
}

bool Gateway::post_status()
{
    ESP_LOGI(TAG, "post_status");
    
    esp_http_client_config_t config {
        .host = "acsgateway.hal9k.dk",
        .path = "/acsstatus",
        .cert_pem = howsmyssl_com_root_cert_pem_start,
        .event_handler = http_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    Http_client_wrapper w(client);

    std::unique_ptr<char[]> buffer;
    size_t size = 0;
    {
        std::lock_guard<std::mutex> g(mutex);
        size = current_status.size();
        if (!size)
        {
            ESP_LOGE(TAG, "GW: No status");
            return false;
        }
        buffer = std::unique_ptr<char[]>(new (std::nothrow) char[size+1]);
        if (!buffer)
        {
            ESP_LOGE(TAG, "GW: Could not allocate %d bytes", size+1);
            return false;
        }
        strcpy(buffer.get(), current_status.c_str());
    }
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, buffer.get(), size);

    const char* content_type = "application/json";
    esp_http_client_set_header(client, "Content-Type", content_type);
    esp_err_t err = esp_http_client_perform(client);

    bool ok = false;
    if (err == ESP_OK)
    {
        ok = true;
        ESP_LOGI(TAG, "GW status = %d", esp_http_client_get_status_code(client));
    }
    else
        ESP_LOGE(TAG, "Error performing http request %s", esp_err_to_name(err));
    
    return ok;
}

void Gateway::check_action()
{
    ESP_LOGI(TAG, "check_action");

    constexpr int HTTP_MAX_OUTPUT = 255;
    char buffer[HTTP_MAX_OUTPUT+1];
    http_max_output = HTTP_MAX_OUTPUT;
    esp_http_client_config_t config {
        .host = "acsgateway.hal9k.dk",
        .path = "/acsquery",
        .cert_pem = howsmyssl_com_root_cert_pem_start,
        .event_handler = http_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .user_data = buffer
    };
    http_output_len = 0;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    Http_client_wrapper w(client);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    auto payload = cJSON_CreateObject();
    cJSON_wrapper jw(payload);
    auto jtoken = cJSON_CreateString(token.c_str());
    cJSON_AddItemToObject(payload, "token", jtoken);

    char* data = cJSON_Print(payload);
    if (!data)
    {
        ESP_LOGI(TAG, "cJSON_Print() returned nullptr");
        return;
    }
    cJSON_Print_wrapper pw(data);
    esp_http_client_set_post_field(client, data, strlen(data));

    const char* content_type = "application/json";
    esp_http_client_set_header(client, "Content-Type", content_type);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "GW status = %d", esp_http_client_get_status_code(client));
        ESP_LOGI(TAG, "GW response = %s", buffer);
        auto root = cJSON_Parse(buffer);
        cJSON_wrapper jw(root);
        if (root)
        {
            auto action_node = cJSON_GetObjectItem(root, "action");
            if (action_node && action_node->type == cJSON_String)
            {
                const std::string action = action_node->valuestring;
                ESP_LOGI(TAG, "GW action = %s", action.c_str());
                // handle action
            }
        }
    }
    else
        ESP_LOGE(TAG, "Error performing http request %s", esp_err_to_name(err));
}

void Gateway::thread_body()
{
    while (1)
    {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        if (!post_status())
        {
            // TODO: Handle loss of connection
        }
        check_action();
    }
}

void gw_task(void*)
{
    Gateway::instance().thread_body();
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
