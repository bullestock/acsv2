#include "defs.h"
#include "gateway.h"
#include "http.h"
#include "nvs.h"

#include "cJSON.h"

#include <string>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_app_desc.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_tls.h"
#include "nvs_flash.h"

#include "esp_http_client.h"

static int output_len;       // Stores number of bytes read

bool set_gw_status(const char* version)
{
    if (get_gateway_token().empty())
    {
        printf("GW: No token\n");
        return false;
    }
    
    char resource[85];
    {
        std::lock_guard<std::mutex> g(relay_mutex);
        sprintf(resource, "/camctl?cameras=%d&estop=%d&version=%s",
                (int) camera_relay_on,
                (int) estop_relay_on,
                version);
    }
    printf("URL: %s\n", resource);
    char buffer[256];
    Http_data http_data;
    http_data.buffer = buffer;
    http_data.max_output = sizeof(buffer) - 1;
    esp_http_client_config_t config {
        .host = "acsgateway.hal9k.dk",
        .path = resource,
        .event_handler = http_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .user_data = &http_data,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    output_len = 0;
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_method(client, HTTP_METHOD_GET);

    char bearer[80];
    snprintf(bearer, sizeof(bearer), "Bearer %s", get_gateway_token().c_str());
    esp_http_client_set_header(client, "Authentication", bearer);
    const char* content_type = "application/json";
    esp_http_client_set_header(client, "Content-Type", content_type);
    esp_err_t err = esp_http_client_perform(client);

    bool ok = false;
    if (err == ESP_OK)
    {
        ok = true;
        ESP_LOGI(TAG, "GW status = %d", esp_http_client_get_status_code(client));
        ESP_LOGI(TAG, "GW response = %s", buffer);
        auto root = cJSON_Parse(buffer);
        if (root)
        {
            auto action_node = cJSON_GetObjectItem(root, "action");
            if (action_node && action_node->type == cJSON_String)
            {
                const std::string action = action_node->valuestring;
                if (action == "on")
                {
                    std::lock_guard<std::mutex> g(relay_mutex);
                    camera_relay_on = true;
                }
                else if (action == "off")
                {
                    std::lock_guard<std::mutex> g(relay_mutex);
                    camera_relay_on = false;
                }
                else if (action == "reboot")
                {
                    vTaskDelay(10000 / portTICK_PERIOD_MS);
                    esp_restart();
                }
            }
        }
    }
    else
        ESP_LOGE(TAG, "Error performing http request %s", esp_err_to_name(err));
    
    esp_http_client_cleanup(client);

    return ok;
}

void gw_task(void*)
{
    const auto app_desc = esp_app_get_description();
    const auto version = app_desc->version;

    int disconnects = 0;
    while (1)
    {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        if (!set_gw_status(version))
        {
            ++disconnects;
            if (disconnects > 5)
            {
                ESP_LOGI(TAG, "REBOOT");
                esp_restart();
            }
        }
    }
}
