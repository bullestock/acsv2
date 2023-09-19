#include "gateway.h"

#include "connect.h"
#include "http.h"

#include "cJSON.h"

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
    std::lock_guard<std::mutex> g(mutex);
    current_status = cJSON_Duplicate(status, true);
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

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    auto payload = cJSON_CreateObject();
    auto jtoken = cJSON_CreateString(token.c_str());
    cJSON_AddItemToObject(payload, "token", jtoken);
    char* data = nullptr;
    {
        std::lock_guard<std::mutex> g(mutex);
        cJSON_AddItemReferenceToObject(payload, "status",
                                       current_status);

        data = strdup(cJSON_Print(payload));
    }
    cJSON_Delete(payload);
    if (!data)
    {
        ESP_LOGE(TAG, "cJSON_Print() returned nullptr");
        return false;
    }
    esp_http_client_set_post_field(client, data, strlen(data));

    const char* content_type = "application/json";
    esp_http_client_set_header(client, "Content-Type", content_type);
    esp_err_t err = esp_http_client_perform(client);
    free(data);

    bool ok = false;
    if (err == ESP_OK)
    {
        ok = true;
        ESP_LOGI(TAG, "GW status = %d", esp_http_client_get_status_code(client));
    }
    else
        ESP_LOGE(TAG, "Error performing http request %s", esp_err_to_name(err));
    
    esp_http_client_cleanup(client);

    return ok;
}

void Gateway::check_action()
{
    ESP_LOGI(TAG, "check_action");

    char buffer[HTTP_MAX_OUTPUT+1];
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

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    auto payload = cJSON_CreateObject();
    auto jtoken = cJSON_CreateString(token.c_str());
    cJSON_AddItemToObject(payload, "token", jtoken);

    const char* data = cJSON_Print(payload);
    if (!data)
    {
        ESP_LOGI(TAG, "cJSON_Print() returned nullptr");
        return;
    }
    esp_http_client_set_post_field(client, data, strlen(data));

    const char* content_type = "application/json";
    esp_http_client_set_header(client, "Content-Type", content_type);
    esp_err_t err = esp_http_client_perform(client);

    cJSON_Delete(payload);

    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "GW status = %d", esp_http_client_get_status_code(client));
        ESP_LOGI(TAG, "GW response = %s", buffer);
        auto root = cJSON_Parse(buffer);
        if (root)
        {
            auto action_node = cJSON_GetObjectItem(root, "action");
            if (action_node && action_node->type == cJSON_String)
            {
                const std::string action = action_node->valuestring;
                ESP_LOGI(TAG, "GW action = %s", action.c_str());
                // handle action
            }
            cJSON_Delete(root);
        }
    }
    else
        ESP_LOGE(TAG, "Error performing http request %s", esp_err_to_name(err));
    
    esp_http_client_cleanup(client);
}

void Gateway::thread_body()
{
    while (1)
    {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        if (current_status)
        {
            if (!post_status())
            {
                // TODO: Handle loss of connection
            }
            check_action();
        }
    }
}

void gw_task(void*)
{
    Gateway::instance().thread_body();
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
