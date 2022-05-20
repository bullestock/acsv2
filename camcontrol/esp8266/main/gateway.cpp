#include "gateway.h"
#include "eventhandler.h"

#include "cJSON.h"

#include <string>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "esp_http_client.h"

static const char* TAG = "CAMCTL";

extern const char howsmyssl_com_root_cert_pem_start[] asm("_binary_howsmyssl_com_root_cert_pem_start");
extern const char howsmyssl_com_root_cert_pem_end[]   asm("_binary_howsmyssl_com_root_cert_pem_end");
extern const char gwtoken_start[] asm("_binary_gwtoken_start");
extern const char gwtoken_end[]   asm("_binary_gwtoken_end");

void set_gw_status()
{
    char resource[85];
    sprintf(resource, "/camctl?active=%d", (int) relay_on.load());
    printf("URL: %s\n", resource);
    char buffer[256];
    esp_http_client_config_t config {
        .host = "acsgateway.hal9k.dk",
        .path = resource,
        .cert_pem = howsmyssl_com_root_cert_pem_start,
        .event_handler = http_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .user_data = buffer
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_method(client, HTTP_METHOD_GET);

    char bearer[80];
    snprintf(bearer, sizeof(bearer), "Bearer %s", gwtoken_start);
    esp_http_client_set_header(client, "Authentication", bearer);
    const char* content_type = "application/json";
    esp_http_client_set_header(client, "Content-Type", content_type);
    esp_err_t err = esp_http_client_perform(client);

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
                if (action == "on")
                {
                    printf("Turn on\n");
                    relay_on = true;
                }
                else if (action == "off")
                {
                    printf("Turn off\n");
                    relay_on = false;
                }
            }
        }
    }
    else
        ESP_LOGE(TAG, "Error performing http request %s", esp_err_to_name(err));
    
    esp_http_client_cleanup(client);
}

void gw_task(void*)
{
    while (1)
    {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        set_gw_status();
    }
}
