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

void Mqtt::set_slack_token(const std::string& token)
{
    slack_token = token;
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

std::string Mqtt::get_present_cards()
{
    std::string s;
    std::lock_guard<std::mutex> g(card_present_mutex);
    for (const auto& e : card_present_status)
    {
        if (e.second)
        {
            if (!s.empty())
                s += ", ";
            s += e.first;
        }
    }
    return s;
}

std::pair<std::string, std::string> Mqtt::get_and_clear_action()
{
    std::lock_guard<std::mutex> g(action_mutex);
    const auto result = std::make_pair(current_action, current_action_arg);
    current_action.clear();
    current_action_arg.clear();
    return result;
}

bool Mqtt::get_allow_open() const
{
    std::lock_guard<std::mutex> g(action_mutex);
    return allow_open;
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

void Mqtt::handle_data(const std::string& full_topic,
                       const std::string& data)
{
    constexpr size_t PREFIX_LEN = strlen("hal9k/acs/");
    if (full_topic.size() < PREFIX_LEN)
        return;
    // Remove "hal9k/acs/" part
    std::string topic = full_topic.substr(PREFIX_LEN);
    if (topic.starts_with("status"))
        handle_status(topic, data);
    else if (topic.starts_with("action"))
        // hal9k/acs/action/tester {"text": "dummy None", "stamp": 1783926100, "hash": "57c5241d4234b91e58f51893270d1480eda0adc8f96b15663fd7b0b5e5ec47d4"}
        handle_action(topic, data);
}

void Mqtt::handle_status(const std::string& topic,
                         const std::string& data)
{
    // Remove "status/" part
    constexpr size_t PREFIX_LEN = strlen("status/");
    if (topic.size() < PREFIX_LEN)
        return;
    const auto device = topic.substr(PREFIX_LEN);
    ESP_LOGI(TAG, "Status device: %s", device.c_str());
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
        // Get door open/unlocked status for other frontend devices
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
        // Get card_present status for bigbro devices
        auto card_present_node = cJSON_GetObjectItem(data_node, "card_present");
        if (card_present_node &&
            ((card_present_node->type == cJSON_False) ||
             (card_present_node->type == cJSON_True)))
        {
            std::lock_guard<std::mutex> g(card_present_mutex);
            card_present_status[device] = card_present_node->type == cJSON_True;
        }
    }
}

void Mqtt::handle_action(const std::string& topic,
                         const std::string& data)
{
    if (topic.contains("/"))
    {
        constexpr size_t PREFIX_LEN = strlen("action/");
        const auto device = topic.substr(PREFIX_LEN);
        ESP_LOGI(TAG, "Action device: %s", device.c_str());
        if (device != get_identifier())
            // Not me
            return;
    }
    if (topic != "action")
    {
        ESP_LOGE(TAG, "Bad global action topic: %s", topic.c_str());
        return;
    }
    auto root = cJSON_Parse(data.c_str());
    cJSON_wrapper jwr(root);
    if (root)
    {
        if (!check_signature(root))
        {
            ESP_LOGE(TAG, "Bad action signature");
            return;
        }
        auto action_node = cJSON_GetObjectItem(root, "action");
        if (action_node && action_node->type == cJSON_String)
        {
            std::string action_arg;
            auto arg_node = cJSON_GetObjectItem(root, "arg");
            if (arg_node && arg_node->type == cJSON_String)
                action_arg = arg_node->valuestring;
            std::lock_guard<std::mutex> g(action_mutex);
            if (action_arg == "open" || action_arg == "close")
            {
                allow_open = action_arg == "open";
                ESP_LOGI(TAG, "allow open: %d", allow_open);
            }
            else
            {
                current_action = action_node->valuestring;
                current_action_arg = action_arg;
                ESP_LOGI(TAG, "action: %s", current_action.c_str());
            }
        }
    }
}

void Mqtt::log(const std::string& msg)
{
    const auto topic = format("hal9k/acs/log/%s", get_identifier().c_str());
    const auto msg_id = esp_mqtt_client_enqueue(client, topic.c_str(),
                                                msg.c_str(), 0, 1, 0, true);
    ESP_LOGI(TAG, "Q log %d", msg_id);
}

void Mqtt::set_status(const char* data,
                      const char* subtopic)
{
    const auto topic = format("hal9k/acs/status/%s",
                              subtopic ? subtopic : get_identifier().c_str());
    const auto msg_id = esp_mqtt_client_enqueue(client, topic.c_str(),
                                                data,
                                                0, 1, 1, true);
    ESP_LOGI(TAG, "Q status %d", msg_id);
}

bool Mqtt::sign(cJSON* payload, const std::string& message)
{
    time_t now;
    time(&now);
    auto stamp = cJSON_CreateNumber(now);
    cJSON_AddItemToObject(payload, "stamp", stamp);

    // Initialize PSA Crypto
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "PSA crypto init failed: %d", status);
        return false;
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
        return false;
    }

    status = psa_hash_update(&hash_op, get_private_key(), SIGNING_KEY_SIZE);
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_hash_update (secret) failed: %d", status);
        psa_hash_abort(&hash_op);
        return false;
    }

    status = psa_hash_update(&hash_op, (const uint8_t*) &now, sizeof(now));
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_hash_update (timestamp) failed: %d", status);
        psa_hash_abort(&hash_op);
        return false;
    }

    status = psa_hash_update(&hash_op, (const uint8_t*) message.c_str(), message.size());
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_hash_update (message) failed: %d", status);
        psa_hash_abort(&hash_op);
        return false;
    }

    status = psa_hash_finish(&hash_op, sha, sizeof(sha), &sha_len);
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_hash_finish failed: %d", status);
        psa_hash_abort(&hash_op);
        return false;
    }

    std::string hex_hash;
    for (int i = 0; i < SIGNING_KEY_SIZE; ++i)
        hex_hash += format("%02x", sha[i]);
    auto hash = cJSON_CreateString(hex_hash.c_str());
    cJSON_AddItemToObject(payload, "hash", hash);

    auto ident = cJSON_CreateString(get_identifier().c_str());
    cJSON_AddItemToObject(payload, "identifier", ident);

    auto text = cJSON_CreateString(message.c_str());
    cJSON_AddItemToObject(payload, "text", text);
    
    return true;
}

bool Mqtt::check_signature(const cJSON* root)
{
    auto stamp_node = cJSON_GetObjectItem(root, "stamp");
    if (!stamp_node || cJSON_IsNumber(stamp_node))
    {
        ESP_LOGE(TAG, "No stamp in data");
        return false;
    }
    auto text_node = cJSON_GetObjectItem(root, "text");
    if (!text_node || stamp_node->type != cJSON_String)
    {
        ESP_LOGE(TAG, "No text in data");
        return false;
    }
    auto hash_node = cJSON_GetObjectItem(root, "hash");
    if (!hash_node || stamp_node->type != cJSON_String)
    {
        ESP_LOGE(TAG, "No hash in data");
        return false;
    }

    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "PSA crypto init failed: %d", status);
        return false;
    }

    uint8_t sha[PSA_HASH_MAX_SIZE];
    size_t sha_len;
    
    psa_hash_operation_t hash_op = PSA_HASH_OPERATION_INIT;
    status = psa_hash_setup(&hash_op, PSA_ALG_SHA_256);
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_hash_setup failed: %d", status);
        psa_hash_abort(&hash_op);
        return false;
    }

    status = psa_hash_update(&hash_op, get_private_key(), SIGNING_KEY_SIZE);
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_hash_update (secret) failed: %d", status);
        psa_hash_abort(&hash_op);
        return false;
    }

    const size_t stamp = stamp_node->valueint;
    status = psa_hash_update(&hash_op, (const uint8_t*) &stamp, sizeof(stamp));
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_hash_update (timestamp) failed: %d", status);
        psa_hash_abort(&hash_op);
        return false;
    }

    const char* message = text_node->valuestring;
    status = psa_hash_update(&hash_op, (const uint8_t*) message, strlen(message));
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_hash_update (message) failed: %d", status);
        psa_hash_abort(&hash_op);
        return false;
    }

    status = psa_hash_finish(&hash_op, sha, sizeof(sha), &sha_len);
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_hash_finish failed: %d", status);
        psa_hash_abort(&hash_op);
        return false;
    }

    std::string hex_hash;
    for (int i = 0; i < SIGNING_KEY_SIZE; ++i)
        hex_hash += format("%02x", sha[i]);
    const char* message_hash = hash_node->valuestring;
    if (!strcmp(hex_hash.c_str(), message_hash))
        return true;
    ESP_LOGE(TAG, "Hash mismatch: %s vs %s", hex_hash.c_str(), message_hash);
    return false;
}

void Mqtt::log_backend(int user_id, const std::string& message)
{
    auto payload = cJSON_CreateObject();
    cJSON_wrapper jw(payload);
    auto uid = cJSON_CreateNumber(user_id);
    cJSON_AddItemToObject(payload, "user_id", uid);

    if (!sign(payload, message))
        return;
    
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
    ESP_LOGI(TAG, "Q backend %d", msg_id);
}

void Mqtt::log_unknown_card(Card_id card_id)
{
    auto payload = cJSON_CreateObject();
    cJSON_wrapper jw(payload);

    if (!sign(payload, format(CARD_ID_FORMAT, card_id)))
        return;
    
    char* data = cJSON_PrintUnformatted(payload);
    if (!data)
    {
        ESP_LOGE(TAG, "cJSON_Print() returned nullptr");
        return;
    }
    cJSON_Print_wrapper pw(data);

    const auto msg_id = esp_mqtt_client_enqueue(client, "hal9k/acs/backend/unknown_card",
                                                data,
                                                0, 1, 0, true);
    ESP_LOGI(TAG, "Q unknown_card %d", msg_id);
}

void Mqtt::write_slack(const std::string& msg,
                       Channel channel)
{
    static const Channel channels[] = { ChannelGeneral, ChannelInfo, ChannelDebug };
    static const char* channel_names[] = {
        "general", "jeg-står-herude-og-banker-på", "private-monitoring"
    };

    for (int i = 0; i < sizeof(channels)/sizeof(Channel); ++i)
    {
        if (!(channel & channels[i]))
            continue;
        const auto channel_name = channel_names[i];

        const std::string message = format("%s|%s", msg.c_str(), channel_name);

        auto payload = cJSON_CreateObject();
        cJSON_wrapper jw(payload);

        if (!sign(payload, message))
            return;
    
        char* data = cJSON_PrintUnformatted(payload);
        if (!data)
        {
            ESP_LOGE(TAG, "cJSON_Print() returned nullptr");
            return;
        }
        cJSON_Print_wrapper pw(data);
    
        const auto msg_id = esp_mqtt_client_enqueue(client, "hal9k/acs/backend/slack",
                                                    data, 0, 1, 0, true);
        ESP_LOGI(TAG, "Q Slack %d", msg_id);
    }
}

void Mqtt::set_slack_status(const std::string& status, bool general)
{
    if (status != last_status)
    {
        write_slack(status, general ? ChannelGeneral : ChannelDebug);
        last_status = status;
    }
}

void Mqtt::slack_announce_open()
{
    set_slack_status(":tada: The space is now open!", true);
}
    
void Mqtt::slack_announce_closed()
{
    set_slack_status(":sad_panda2: The space is no longer open", true);
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
