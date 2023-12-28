#include "gateway.h"

#include "defs.h"
#include "display.h"
#include "http.h"
#include "logger.h"
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
#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_tls.h>
#include <nvs_flash.h>

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

std::string Gateway::get_and_clear_action()
{
    std::lock_guard<std::mutex> g(mutex);
    const auto action = current_action;
    current_action.clear();
    return action;
}

bool Gateway::post_status()
{
    std::unique_ptr<char[]> buffer;
    size_t size = 0;
    {
        std::lock_guard<std::mutex> g(mutex);
        size = current_status.size();
        if (!size)
            return false;
        buffer = std::unique_ptr<char[]>(new (std::nothrow) char[size+1]);
        if (!buffer)
        {
            ESP_LOGE(TAG, "GW: Could not allocate %d bytes", size+1);
            return false;
        }
        strcpy(buffer.get(), current_status.c_str());
    }

    esp_http_client_config_t config {
        .host = "acsgateway.hal9k.dk",
        .path = "/acsstatus",
        .cert_pem = howsmyssl_com_root_cert_pem_start,
        .event_handler = http_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    Http_client_wrapper w(client);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, buffer.get(), size);

    const char* content_type = "application/json";
    esp_http_client_set_header(client, "Content-Type", content_type);
    esp_err_t err = esp_http_client_perform(client);

    const bool ok = err == ESP_OK;
    if (ok)
        ESP_LOGI(TAG, "GW post_status: HTTP %d", esp_http_client_get_status_code(client));
    else
        ESP_LOGE(TAG, "GW: post_status: error %s", esp_err_to_name(err));
    
    return ok;
}

void Gateway::check_action()
{
    constexpr int HTTP_MAX_OUTPUT = 255;
    char buffer[HTTP_MAX_OUTPUT+1];
    Http_data http_data;
    http_data.buffer = buffer;
    http_data.max_output = HTTP_MAX_OUTPUT;
    esp_http_client_config_t config {
        .host = "acsgateway.hal9k.dk",
        .path = "/acsquery",
        .cert_pem = howsmyssl_com_root_cert_pem_start,
        .event_handler = http_event_handler,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .user_data = &http_data
    };
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

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "GW: check_action: error %s", esp_err_to_name(err));
        return;
    }

    const int code = esp_http_client_get_status_code(client);
    if (code != 200)
    {
        ESP_LOGI(TAG, "GW: check_action: HTTP %d", code);
        return;
    }
    auto root = cJSON_Parse(buffer);
    cJSON_wrapper jwr(root);
    if (root)
    {
        auto action_node = cJSON_GetObjectItem(root, "action");
        if (action_node && action_node->type == cJSON_String)
        {
            current_action = action_node->valuestring;
            ESP_LOGI(TAG, "GW action = %s", current_action.c_str());
        }
    }
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

bool Gateway::upload_coredump(Display& display)
{
    const esp_partition_t* p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                       ESP_PARTITION_SUBTYPE_DATA_COREDUMP,
                                                       NULL);
    if (!p)
    {
        printf("No coredump partition found\n");
        return false;
    }
    char stamp[Logger::TIMESTAMP_SIZE];
    Logger::make_timestamp(stamp);

    printf("Coredump partition size %ld bytes\n", (long) p->size);
    time_t start;
    time(&start);
    constexpr size_t BUF_SIZE = 768*8*2;
    auto buf = std::make_unique<uint8_t[]>(BUF_SIZE);
    size_t remaining = p->size;
    size_t offset = 0;
    while (1)
    {
        const auto chunk_size = std::min(BUF_SIZE, remaining);
        if (!chunk_size)
            break;
        esp_err_t e = esp_partition_read(p, offset, buf.get(), chunk_size);
        if (e != ESP_OK)
        {
            Logger::instance().log_sync(stamp, "Error reading partition");
            return false;
        }
        // Check for empty partition
        if (!offset)
        {
            bool non_empty = false;
            for (int i = 0; i < 32; ++i)
                if (buf[i] != 0xFF)
                {
                    non_empty = true;
                    break;
                }
            if (!non_empty)
            {
                printf("Coredump partition is empty\n");
                return true;
            }
            display.add_progress("Uploading core dump");
            Logger::instance().log_sync(stamp, "================= CORE DUMP START =================");
        }
        size_t needed_size = 0;
        mbedtls_base64_encode(nullptr, 0, &needed_size, buf.get(), chunk_size);
        auto out_buf = std::make_unique<uint8_t[]>(needed_size);
        size_t encoded_size = 0;
        int err = mbedtls_base64_encode(out_buf.get(), needed_size, &encoded_size, buf.get(), chunk_size);
        if (err)
        {
            Logger::instance().log_sync(stamp, "Base64 error");
            return false;
        }
        Logger::instance().log_sync(stamp, reinterpret_cast<char*>(out_buf.get()));
        remaining -= chunk_size;
        offset += chunk_size;
    }
    Logger::instance().log_sync(stamp, "================= CORE DUMP END ===================");

    time_t end;
    time(&end);

    printf("Dumped in %d s\n", int(end - start));
    
    ESP_ERROR_CHECK(esp_partition_erase_range(p, 0, p->size));

    Slack_writer::instance().send_message(":ladybug: Uploaded core dump");

    return true;
}

void gw_task(void*)
{
    Gateway::instance().thread_body();
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
