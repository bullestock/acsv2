#include "cardcache.h"

#include "defs.h"
#include "format.h"
#include "http.h"
#include "logger.h"
#include "util.h"

#include "cJSON.h"

#include "esp_log.h"

extern const char howsmyssl_com_root_cert_pem_start[] asm("_binary_howsmyssl_com_root_cert_pem_start");
extern const char howsmyssl_com_root_cert_pem_end[]   asm("_binary_howsmyssl_com_root_cert_pem_end");

constexpr util::duration MAX_CACHE_AGE = std::chrono::minutes(15);

Card_cache& Card_cache::instance()
{
    static Card_cache the_instance;
    return the_instance;
}

void Card_cache::set_api_token(const std::string& token)
{
    api_token = token;
}

Card_cache::Result Card_cache::has_access(Card_cache::Card_id id)
{
    User_info ui;
    bool found = false;
    {
        std::lock_guard<std::mutex> g(cache_mutex);
        const auto it = cache.find(id);
        if (it != cache.end())
        {
            found = true;
            ui = it->second;
        }
    }
    if (found)
    {
        if (util::now() - ui.last_update < MAX_CACHE_AGE)
        {
            Logger::instance().log(format("%010X cached", id));
            Logger::instance().log_backend(ui.user_id, "Granted entry");
            return Result(Access::Allowed, ui.user_int_id);
        }
        Logger::instance().log(format("%010X: stale", id));
        // Cache entry is outdated
    }

    if (api_token.empty())
    {
        ESP_LOGE(TAG, "Card_cache: no API token");
        return Result(Access::Error);
    }

    constexpr int HTTP_MAX_OUTPUT = 255;
    char* buffer = reinterpret_cast<char*>(malloc(HTTP_MAX_OUTPUT+1));
    http_max_output = HTTP_MAX_OUTPUT;
    esp_http_client_config_t config {
        .host = "panopticon.hal9k.dk",
        .path = "/api/v1/permissions",
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
    auto jtoken = cJSON_CreateString(api_token.c_str());
    cJSON_AddItemToObject(payload, "api_token", jtoken);
    auto card_id = cJSON_CreateString(format("%010X", id).c_str());
    cJSON_AddItemToObject(payload, "card_id", card_id);

    const char* data = cJSON_Print(payload);
    if (!data)
    {
        ESP_LOGE(TAG, "Card_cache: cJSON_Print() returned nullptr");
        free(buffer);
        return Result(Access::Error);
    }
    esp_http_client_set_post_field(client, data, strlen(data));

    const char* content_type = "application/json";
    esp_http_client_set_header(client, "Accept", content_type);
    esp_http_client_set_header(client, "Content-Type", content_type);
    esp_err_t err = esp_http_client_perform(client);

    Result res(Access::Error);
    if (err == ESP_OK)
        res = get_result(client, buffer, id);
    else
        ESP_LOGE(TAG, "HTTP error %s for logs", esp_err_to_name(err));

    free(buffer);

    return res;
}

Card_cache::Card_id get_id_from_string(const std::string& s)
{
    std::istringstream is(s);
    Card_cache::Card_id id = 0;
    is >> std::hex >> id;
    return id;
}

Card_cache::Result Card_cache::has_access(const std::string& sid)
{
    return has_access(get_id_from_string(sid));
}

void Card_cache::thread_body()
{
    while (1)
    {
        if (api_token.empty())
        {
            ESP_LOGE(TAG, "Card_cache: no API token");
            vTaskDelay(60000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "Update card cache");

        // Fetch card info
        constexpr int HTTP_MAX_OUTPUT = 10*1024;
        http_max_output = HTTP_MAX_OUTPUT;
        char* buffer = reinterpret_cast<char*>(malloc(HTTP_MAX_OUTPUT+1));
        if (!buffer)
        {
            ESP_LOGE(TAG, "Could not allocate %d bytes", HTTP_MAX_OUTPUT+1);
            continue;
        }
        esp_http_client_config_t config {
            .host = "panopticon.hal9k.dk",
            .path = "/api/v2/permissions/",
            .cert_pem = howsmyssl_com_root_cert_pem_start,
            .event_handler = http_event_handler,
            .transport_type = HTTP_TRANSPORT_OVER_SSL,
            .user_data = buffer
        };
        http_output_len = 0;
        esp_http_client_handle_t client = esp_http_client_init(&config);
        Http_client_wrapper w(client);

        ESP_ERROR_CHECK(esp_http_client_set_method(client, HTTP_METHOD_GET));

        const char* content_type = "application/json";
        ESP_ERROR_CHECK(esp_http_client_set_header(client, "Accept", content_type));
        ESP_ERROR_CHECK(esp_http_client_set_header(client, "Content-Type", content_type));
        const std::string auth = std::string("Token ") + std::string(api_token);
        ESP_ERROR_CHECK(esp_http_client_set_header(client, "Authorization", auth.c_str()));
        esp_err_t err = esp_http_client_perform(client);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "HTTP error %s for /v2/permissions", esp_err_to_name(err));
            continue;
        }
        const auto code = esp_http_client_get_status_code(client);
        if (code != 200)
        {
            ESP_LOGE(TAG, "Error: Unexpected response from /v2/permissions: %d", code);
            Logger::instance().log(format("Error: Unexpected response from /v2/permissions: %d", code));
            free(buffer);
            continue;
        }
        auto root = cJSON_Parse(buffer);
        cJSON_wrapper jw(root);
        if (!root)
        {
            ESP_LOGE(TAG, "Error: Bad JSON from /v2/permissions: %s", buffer);
            free(buffer);
            Logger::instance().log(format("Error: Bad JSON from /v2/permissions"));
            continue;
        }
        free(buffer);
        if (!cJSON_IsArray(root))
        {
            ESP_LOGE(TAG, "Error: Response from /v2/permissions is not an array");
            Logger::instance().log("Error: Response from /v2/permissions is not an array");
            continue;
        }
        // Create new cache
        Cache new_cache;
        cJSON* it;
        cJSON_ArrayForEach(it, root)
        {
            if (!cJSON_IsObject(it))
            {
                ESP_LOGE(TAG, "Error: Item from /v2/permissions is not an object");
                Logger::instance().log("Error: Item from /v2/permissions is not an object");
                continue;
            }
            auto card_id_node = cJSON_GetObjectItem(it, "card_id");
            if (!cJSON_IsString(card_id_node))
            {
                ESP_LOGE(TAG, "Error: Item from /v2/permissions has no card_id");
                Logger::instance().log("Error: Item from /v2/permissions has no card_id");
                continue;
            }
            auto id_node = cJSON_GetObjectItem(it, "id");
            if (!cJSON_IsNumber(id_node))
            {
                ESP_LOGE(TAG, "Error: Item from /v2/permissions has no id");
                Logger::instance().log("Error: Item from /v2/permissions has no id");
                continue;
            }
            auto int_id_node = cJSON_GetObjectItem(it, "int_id");
            if (!cJSON_IsNumber(int_id_node))
            {
                ESP_LOGE(TAG, "Error: Item from /v2/permissions has no int_id");
                Logger::instance().log("Error: Item from /v2/permissions has no int_id");
                continue;
            }
            const auto card_id = get_id_from_string(card_id_node->valuestring);
            const auto id = id_node->valueint;
            const auto int_id = int_id_node->valueint;
            ESP_LOGI(TAG, "Cache: %010llu, %d, %d", card_id, id, int_id);
            new_cache[card_id] = { id, int_id, util::now() };
        }
        // Store
        const auto size = new_cache.size();
        {
            std::lock_guard<std::mutex> g(cache_mutex);
            cache.swap(new_cache);
        }
        Logger::instance().log(format("Card cache updated: %d cards", static_cast<int>(size)));

        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
}

Card_cache::Result Card_cache::get_result(esp_http_client_handle_t client, const char* buffer, int id)
{
    const auto code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "Card backend status = %d", code);
    if (code != 200)
        return Result(code == 404 ? Access::Unknown : Access::Error, -1);
    auto root = cJSON_Parse(buffer);
    cJSON_wrapper jw(root);
    if (!root)
        return Result(Access::Error, -1);
    auto allowed = cJSON_GetObjectItem(root, "allowed");
    if (!allowed || !cJSON_IsNumber(allowed))
        return Result(Access::Error, -1);

    int user_int_id = -1;
    if (allowed->valueint)
    {
        const int user_id = cJSON_GetObjectItem(root, "id")->valueint;
        user_int_id = cJSON_GetObjectItem(root, "int_id")->valueint;
        cache[id] = { user_id, user_int_id, util::now() };
        Logger::instance().log_backend(user_id, "Granted entry");
    }
    return Result(allowed->valueint ? Access::Allowed : Access::Forbidden, user_int_id);
}

void card_cache_task(void*)
{
    Card_cache::instance().thread_body();
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
