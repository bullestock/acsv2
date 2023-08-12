#include "gateway.h"
#include "connect.h"
#include "gpio.h"
#include "led.h"

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

static const char* TAG = "CAMCTL";

extern const char howsmyssl_com_root_cert_pem_start[] asm("_binary_howsmyssl_com_root_cert_pem_start");
extern const char howsmyssl_com_root_cert_pem_end[]   asm("_binary_howsmyssl_com_root_cert_pem_end");
extern const char gwtoken_start[] asm("_binary_gwtoken_start");
extern const char gwtoken_end[]   asm("_binary_gwtoken_end");

static int output_len;       // Stores number of bytes read

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
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
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (evt->user_data) {
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
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error(reinterpret_cast<esp_tls_error_handle_t>(evt->data), &mbedtls_err, nullptr);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            break;
    }
    return ESP_OK;
}

bool set_gw_status()
{
    char resource[85];
    sprintf(resource, "/camctl?active=%d", (int) relay_on);
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
    output_len = 0;
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_method(client, HTTP_METHOD_GET);

    char bearer[80];
    snprintf(bearer, sizeof(bearer), "Bearer %s", gwtoken_start);
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
                    set_led_camera(true);
                }
                else if (action == "off")
                {
                    printf("Turn off\n");
                    relay_on = false;
                    set_led_camera(false);
                }
            }
        }
    }
    else
        ESP_LOGE(TAG, "Error performing http request %s", esp_err_to_name(err));
    
    esp_http_client_cleanup(client);

    return ok;
}

bool connect()
{
    disconnect();
    if (connect("hal9k"))
        return true;
    disconnect();
    return connect("bullestock-guest");
}

void gw_task(void*)
{
    init_wifi();

    set_led_online_flash(100, 1000);
    while (!connect())
        ;
    set_led_online_steady(true);
    
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
            if (connect())
                set_led_online(true);
        }
    }
}
