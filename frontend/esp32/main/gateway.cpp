#include "gateway.h"
#include "connect.h"

#include "cJSON.h"

#include <string>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_tls.h"
#include "nvs_flash.h"

#include "esp_http_client.h"

extern const char howsmyssl_com_root_cert_pem_start[] asm("_binary_howsmyssl_com_root_cert_pem_start");
extern const char howsmyssl_com_root_cert_pem_end[]   asm("_binary_howsmyssl_com_root_cert_pem_end");

static const int MAX_OUTPUT = 255;

static int output_len;       // Stores number of bytes read

esp_err_t http_event_handler(esp_http_client_event_t* evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        /*
         *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
         *  However, event handler can also be used in case chunked encoding is used.
         */
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            if (evt->user_data)
            {
                if (output_len + evt->data_len >= MAX_OUTPUT)
                {
                    ESP_LOGI(TAG, "HTTP buffer overflow");
                    break;
                }
                auto p = reinterpret_cast<char*>(evt->user_data);
                memcpy(p + output_len, evt->data, evt->data_len);
                output_len += evt->data_len;
                p[output_len] = 0;
            }
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_REDIRECT:
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error(reinterpret_cast<esp_tls_error_handle_t>(evt->data), &mbedtls_err, nullptr);
        if (err)
        {
            ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
            ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
        }
        break;
    }
    return ESP_OK;
}

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
        ESP_LOGI(TAG, "cJSON_Print() returned nullptr");
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

    char buffer[MAX_OUTPUT+1];
    esp_http_client_config_t config {
        .host = "acsgateway.hal9k.dk",
        .path = "/acsquery",
        .cert_pem = howsmyssl_com_root_cert_pem_start,
        .event_handler = http_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .user_data = buffer
    };
    output_len = 0;
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
