#include "foreninglet.h"

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

ForeningLet& ForeningLet::instance()
{
    static ForeningLet the_instance;
    return the_instance;
}

void ForeningLet::set_credentials(const std::string& username, const std::string& password)
{
    forening_let_user = username;
    forening_let_password = password;
}

void ForeningLet::update_last_access(int user_id, const util::time_point& timestamp)
{
    Item item{ user_id };
    time_t current = 0;
    time(&current);
    struct tm timeinfo;
    gmtime_r(&current, &timeinfo);
    strftime(item.stamp, sizeof(item.stamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
    std::lock_guard<std::mutex> g(mutex);
    if (q.size() > 100)
    {
        ESP_LOGE(TAG, "ForeningLet: Queue overflow");
        return;
    }
    q.push_back(item);
}

void ForeningLet::thread_body()
{
    Item item;
    while (1)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);

        if (q.empty())
            continue;
        item = q.back();
        q.pop_back();

        if (forening_let_user.empty() || forening_let_password.empty())
        {
            ESP_LOGE(TAG, "ForeningLet: Missing credentials");
            continue;
        }

        const auto path = format("/api/member/id/%d/?version=1", item.user_id);
        esp_http_client_config_t config {
            .host = "foreninglet.dk",
            .username = forening_let_user.c_str(),
            .password = forening_let_password.c_str(),
            .auth_type = HTTP_AUTH_TYPE_BASIC,
            .path = path.c_str(),
            .cert_pem = howsmyssl_com_root_cert_pem_start,
            .event_handler = http_event_handler,
            .transport_type = HTTP_TRANSPORT_OVER_SSL,
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);
        Http_client_wrapper w(client);
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_method(client, HTTP_METHOD_PUT);
        
        auto payload = cJSON_CreateObject();
        cJSON_wrapper jw(payload);
        auto members = cJSON_CreateObject();
        auto field1 = cJSON_CreateString(item.stamp);
        cJSON_AddItemToObject(members, "field1", field1);
        cJSON_AddItemToObject(payload, "members", members);

        char* data = cJSON_Print(payload);
        if (!data)
        {
            ESP_LOGE(TAG, "ForeningLet: cJSON_Print() returned nullptr");
            break;
        }
        cJSON_Print_wrapper pw(data);
        esp_http_client_set_post_field(client, data, strlen(data));
        esp_err_t err = esp_http_client_perform(client);

        if (err == ESP_OK)
            ESP_LOGI(TAG, "ForeningLet status = %d", esp_http_client_get_status_code(client));
        else
            ESP_LOGE(TAG, "HTTP error %s for foreninglet", esp_err_to_name(err));
    }
}

void foreninglet_task(void*)
{
    ForeningLet::instance().thread_body();
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
