#include "http.h"

#include "defs.h"

#include "esp_log.h"
#include "esp_tls.h"

int http_max_output = 0;
int http_output_len = 0;       // Stores number of bytes read

esp_err_t http_event_handler(esp_http_client_event_t* evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        //ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        //ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        //ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        //ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        //ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            if (evt->user_data)
            {
                auto http_data = reinterpret_cast<Http_data*>(evt->user_data);
                if (http_data->output_len + evt->data_len >= http_data->max_output)
                {
                    ESP_LOGE(TAG, "HTTP buffer overflow: %d/%d", http_data->output_len + evt->data_len, http_data->max_output);
                    break;
                }
                auto p = http_data->buffer;
                memcpy(p + http_data->output_len, evt->data, evt->data_len);
                http_data->output_len += evt->data_len;
                p[http_data->output_len] = 0;
            }
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        //ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_REDIRECT:
        break;
    case HTTP_EVENT_DISCONNECTED:
        //ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
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

Http_client_wrapper::Http_client_wrapper(esp_http_client_handle_t& handle)
    : handle(handle)
{
}

Http_client_wrapper::~Http_client_wrapper()
{
    esp_http_client_close(handle);
    esp_http_client_cleanup(handle);
}

std::mutex http_mutex;
