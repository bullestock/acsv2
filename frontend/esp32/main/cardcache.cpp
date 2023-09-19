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

Card_cache::Result Card_cache::has_access(Card_cache::Card_id id)
{
    std::lock_guard<std::mutex> g(cache_mutex);
    const auto it = cache.find(id);
    if (it != cache.end())
    {
        if (util::now() - it->second.last_update < MAX_CACHE_AGE)
        {
            Logger::instance().log(format("%010X cached", id));
            Logger::instance().log_backend(it->second.user_id, "Granted entry");
            return Result(Access::Allowed, it->second.user_int_id);
        }
        Logger::instance().log(format("%010X: stale", id));
        // Cache entry is outdated
    }

    char buffer[HTTP_MAX_OUTPUT+1];
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

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    auto payload = cJSON_CreateObject();
    auto jtoken = cJSON_CreateString(api_token.c_str());
    cJSON_AddItemToObject(payload, "api_token", jtoken);
    auto card_id = cJSON_CreateString(format("%010X", id).c_str());
    cJSON_AddItemToObject(payload, "card_id", card_id);

    const char* data = cJSON_Print(payload);
    if (!data)
    {
        ESP_LOGI(TAG, "Logger: cJSON_Print() returned nullptr");
        return Result(Access::Error);
    }
    esp_http_client_set_post_field(client, data, strlen(data));

    const char* content_type = "application/json";
    esp_http_client_set_header(client, "Accept", content_type);
    esp_http_client_set_header(client, "Content-Type", content_type);
    esp_err_t err = esp_http_client_perform(client);
    cJSON_Delete(payload);
    Result res(Access::Error);
    if (err == ESP_OK)
        res = get_result(client, buffer, id);
    else
        ESP_LOGE(TAG, "HTTP error %s for logs", esp_err_to_name(err));

    esp_http_client_cleanup(client);

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

void Card_cache::update_cache()
{
    while (1)
    {
        vTaskDelay(60000 / portTICK_PERIOD_MS);
        // Fetch card info
        /*
          RestClient::Connection conn(BASE_URL);
          conn.SetTimeout(5);
          conn.AppendHeader("Content-Type", "application/json");
          conn.AppendHeader("Accept", "application/json");
          conn.AppendHeader("Authorization", "Token "+api_token);
          const auto resp = conn.get("/v2/permissions/");
          Logger::instance().log_verbose(fmt::format("resp.code: {}", resp.code));
          Logger::instance().log_verbose(fmt::format("resp.body: {}", resp.body));
          if (resp.code != 200)
          {
          Logger::instance().log(fmt::format("Error: Unexpected response from /v2/permissions: {}", resp.code));
          continue;
          }
          const auto resp_body = util::json::parse(resp.body);
          if (!resp_body.is_array())
          {
          Logger::instance().log("Error: Response from /v2/permissions is not an array");
          continue;
          }
          // Create new cache
          Cache new_cache;
          for (const auto& elem : resp_body)
          try
          {
          new_cache[get_id_from_string(elem.at("card_id").get<std::string>())] = {
          elem.at("id").get<int>(),
          elem.at("int_id").get<int>(),
          util::now()
          };
          }
          catch (const std::exception& e)
          {
          Logger::instance().log(fmt::format("Error: JSON exception {} in {}", e.what(), elem.dump()));
          }
          {
          // Store
          std::lock_guard<std::mutex> g(cache_mutex);
          cache.swap(new_cache);
          }
        */
        Logger::instance().log("Card cache updated");
    }
}

Card_cache::Result Card_cache::get_result(esp_http_client_handle_t client, const char* buffer, int id)
{
    const auto code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "Card backend status = %d", code);
    if (code != 200)
        return Result(code == 404 ? Access::Unknown : Access::Error, -1);
    auto root = cJSON_Parse(buffer);
    if (!root)
        return Result(Access::Error, -1);
    auto allowed = cJSON_GetObjectItem(root, "allowed");
    if (!allowed || !cJSON_IsNumber(allowed))
    {
        cJSON_Delete(root);
        return Result(Access::Error, -1);
    }
    int user_int_id = -1;
    if (allowed->valueint)
    {
        const int user_id = cJSON_GetObjectItem(root, "id")->valueint;
        user_int_id = cJSON_GetObjectItem(root, "int_id")->valueint;
        cache[id] = { user_id, user_int_id, util::now() };
        Logger::instance().log_backend(user_id, "Granted entry");
    }
    cJSON_Delete(root);
    return Result(allowed->valueint ? Access::Allowed : Access::Forbidden, user_int_id);
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
