#include "esp_system.h"
#include "esp_event.h"
#include "psa/crypto.h"

#include <string>

#include "cJSON.h"
#include "defs.h"
#include "format.h"
#include "mqtt.h"
#include "nvs.h"

static constexpr const char* TAG = "mqtt";

Mqtt& Mqtt::instance()
{
    static Mqtt the_instance;
    return the_instance;
}

std::string Mqtt::get_open_doors()
{
    std::string s;
    std::lock_guard<std::mutex> g(door_status_mutex);
    for (const auto& e : door_status)
    {
        if (e.second.first || e.second.second)
        {
            if (!s.empty())
                s += ", ";
            if (e.second.first)
                // Door is open
                s += "*";
            s += e.first;
        }
    }
    return s;
}

void Mqtt::event_handler(void* handler_args,
                         esp_event_base_t base,
                         int32_t event_id,
                         void* event_data)
{
    Mqtt* self = reinterpret_cast<Mqtt*>(handler_args);
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    auto event = reinterpret_cast<esp_mqtt_event_handle_t>(event_data);
    switch ((esp_mqtt_event_id_t) event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected");
        self->connected = true;
        esp_mqtt_client_subscribe(event->client, "hal9k/acs/status/#", 1);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected");
        self->connected = false;
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "Pub %d", event->msg_id);
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "Sub %d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "Got data");
        self->handle_data(std::string(event->topic, event->topic_len),
                          std::string(event->data, event->data_len));
        break;
        
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "Error %d", event->msg_id);
        break;

    default:
        ESP_LOGI(TAG, "Other event: %d", event->event_id);
        break;
    }
}

void Mqtt::handle_data(const std::string& topic,
                       const std::string& data)
{
    // Remove "hal9k/acs/status/" part
    constexpr size_t PREFIX_LEN = strlen("hal9k/acs/status/");
    if (topic.size() < PREFIX_LEN)
        return;
    const auto device = topic.substr(PREFIX_LEN);
    ESP_LOGI(TAG, "Device: %s", device.c_str());
    if (device == get_identifier())
        // Skip myself
        return;
    auto root = cJSON_Parse(data.c_str());
    cJSON_wrapper jwr(root);
    if (!root)
    {
        ESP_LOGE(TAG, "Bad JSON: %s", data.c_str());
        return;
    }
    auto data_node = cJSON_GetObjectItem(root, "data");
    if (data_node && data_node->type == cJSON_Object)
    {
        auto door_node = cJSON_GetObjectItem(data_node, "door");
        auto lock_status_node = cJSON_GetObjectItem(data_node, "lock_status");
        if (door_node && door_node->type == cJSON_String)
        {
            const bool is_door_open = !strcmp(door_node->valuestring, "open");
            ESP_LOGI(TAG, "Door open: %d", is_door_open);
            if (lock_status_node && lock_status_node->type == cJSON_String)
            {
                const bool is_unlocked = !strcmp(lock_status_node->valuestring, "unlocked");
                ESP_LOGI(TAG, "Unlocked: %d", is_unlocked);
                std::lock_guard<std::mutex> g(door_status_mutex);
                door_status[device] = std::make_pair(is_door_open, is_unlocked);
            }
        }
    }
}

void Mqtt::log(const std::string& msg)
{
    const auto topic = format("hal9k/acs/log/%s", get_identifier().c_str());
    const auto msg_id = esp_mqtt_client_enqueue(client, topic.c_str(),
                                                msg.c_str(), 0, 1, 0, true);
    ESP_LOGI(TAG, "Q %d", msg_id);
}

void Mqtt::set_status(const char* data,
                      const char* subtopic)
{
    const auto topic = format("hal9k/acs/status/%s",
                              subtopic ? subtopic : get_identifier().c_str());
    const auto msg_id = esp_mqtt_client_enqueue(client, topic.c_str(),
                                                data,
                                                0, 1, 1, true);
    ESP_LOGI(TAG, "Q %d", msg_id);
}

void Mqtt::log_backend(int user_id, const std::string& message)
{
    auto payload = cJSON_CreateObject();
    cJSON_wrapper jw(payload);
    auto ident = cJSON_CreateString(get_identifier().c_str());
    cJSON_AddItemToObject(payload, "identifier", ident);
    auto uid = cJSON_CreateNumber(user_id);
    cJSON_AddItemToObject(payload, "user_id", uid);
    auto text = cJSON_CreateString(message.c_str());
    cJSON_AddItemToObject(payload, "text", text);

    time_t now;
    time(&now);
    auto stamp = cJSON_CreateNumber(now);
    cJSON_AddItemToObject(payload, "stamp", stamp);

    // Initialize PSA Crypto
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "PSA crypto init failed: %d", status);
        return;
    }

    // Compute SHA256 of secret + timestamp + message using PSA API
    uint8_t sha[PSA_HASH_MAX_SIZE];
    size_t sha_len;
    
    psa_hash_operation_t hash_op = PSA_HASH_OPERATION_INIT;
    status = psa_hash_setup(&hash_op, PSA_ALG_SHA_256);
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_hash_setup failed: %d", status);
        psa_hash_abort(&hash_op);
        return;
    }

    status = psa_hash_update(&hash_op, get_private_key(), AES_KEY_SIZE);
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_hash_update (secret) failed: %d", status);
        psa_hash_abort(&hash_op);
        return;
    }

    status = psa_hash_update(&hash_op, (const uint8_t*) &now, sizeof(now));
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_hash_update (timestamp) failed: %d", status);
        psa_hash_abort(&hash_op);
        return;
    }

    status = psa_hash_update(&hash_op, (const uint8_t*) message.c_str(), message.size());
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_hash_update (message) failed: %d", status);
        psa_hash_abort(&hash_op);
        return;
    }

    status = psa_hash_finish(&hash_op, sha, sizeof(sha), &sha_len);
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_hash_finish failed: %d", status);
        psa_hash_abort(&hash_op);
        return;
    }

    std::string hex_hash;
    for (auto c : sha)
        hex_hash += format("%02x", c);
    auto hash = cJSON_CreateString(hex_hash.c_str());
    cJSON_AddItemToObject(payload, "hash", hash);
    
    char* data = cJSON_PrintUnformatted(payload);
    if (!data)
    {
        ESP_LOGE(TAG, "cJSON_Print() returned nullptr");
        return;
    }
    cJSON_Print_wrapper pw(data);

    const auto msg_id = esp_mqtt_client_enqueue(client, "hal9k/acs/backend/log",
                                                data,
                                                0, 1, 0, true);
    ESP_LOGI(TAG, "Q %d", msg_id);
}

void Mqtt::log_unknown_card(Card_id card_id)
{
}

void Mqtt::start(const std::string& mqtt_address)
{
    std::string mqtt_url = std::string("mqtt://") + mqtt_address;
    esp_mqtt_client_config_t mqtt_cfg = {
    };
    ESP_LOGI(TAG, "URL %s", mqtt_url.c_str());
    mqtt_cfg.broker.address.uri = mqtt_url.c_str();
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client,
                                   static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
                                   &Mqtt::event_handler, this);
    esp_mqtt_client_start(client);
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
