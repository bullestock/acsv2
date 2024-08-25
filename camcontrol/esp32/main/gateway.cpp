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
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_tls.h"
#include "nvs_flash.h"

#include "esp_http_client.h"

static int output_len;       // Stores number of bytes read

bool set_gw_status()
{
    if (get_gateway_token().empty())
    {
        printf("GW: No token\n");
        return false;
    }
    
    char resource[85];
    sprintf(resource, "/camctl?active=%d", (int) relay_on);
    printf("URL: %s\n", resource);
    char buffer[256];
    esp_http_client_config_t config {
        .host = "acsgateway.hal9k.dk",
        .path = resource,
        .event_handler = http_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .user_data = buffer,
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
                    printf("Turn on\n");
                    relay_on = true;
                    //set_led_camera(true);
                }
                else if (action == "off")
                {
                    printf("Turn off\n");
                    relay_on = false;
                    //set_led_camera(false);
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
    int disconnects = 0;
    while (1)
    {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        if (!set_gw_status())
        {
            ++disconnects;
            if (disconnects > 5)
            {
                nvs_handle my_handle;
                ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
                const auto ret = nvs_set_i8(my_handle, "relay", (int8_t) relay_on);
                ESP_LOGI(TAG, "Save: %d", ret);
                nvs_close(my_handle);
                ESP_LOGI(TAG, "REBOOT");
                esp_restart();
            }
        }
    }
}
