#include "esp_system.h"
#include "esp_event.h"

#include <string>

#include "mqtt_client.h"

#include "cJSON.h"
#include "defs.h"
#include "format.h"
#include "mqtt.h"
#include "nvs.h"

static bool connected = false;
static esp_mqtt_client_handle_t client = 0;

#define TAG "mqtt"

static void handle_data(const std::string& topic,
                        const std::string& data);

static void mqtt_event_handler(void* handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void* event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    auto event = reinterpret_cast<esp_mqtt_event_handle_t>(event_data);
    switch ((esp_mqtt_event_id_t) event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected");
        connected = true;
        esp_mqtt_client_subscribe(event->client, "hal9k/acs/status/#", 1);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected");
        connected = false;
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "Published %d", event->msg_id);
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "Subscribed %d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "Data");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        handle_data(std::string(event->topic, event->topic_len),
                    std::string(event->data, event->data_len));
        break;
        
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "ERROR: %d", event->msg_id);
        break;

    default:
        ESP_LOGI(TAG, "Other event: %d", event->event_id);
        break;
    }
}

static void handle_data(const std::string& topic,
                        const std::string& data)
{
    // Remove "hal9k/acs/status/" part
    auto t = topic.substr(strlen("hal9k/acs/status/"));
    const auto slash = t.find("/");
    if (slash == std::string::npos)
    {
        ESP_LOGE(TAG, "Bad topic: %s", topic.c_str());
        return;
    }
    const auto device = t.substr(0, slash);
    ESP_LOGI(TAG, "Device: %s", device.c_str());
    if (device == get_identifier())
    {
        ESP_LOGI(TAG, "It me");
        return;
    }
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
        if (door_node && door_node->type == cJSON_String)
        {
            const bool is_door_open = !strcmp(door_node->valuestring, "open");
            ESP_LOGI(TAG, "Door open: %d", is_door_open);
        }
        auto lock_status_node = cJSON_GetObjectItem(data_node, "lock_status");
        if (lock_status_node && lock_status_node->type == cJSON_String)
        {
            const bool is_unlocked = !strcmp(lock_status_node->valuestring, "unlocked");
            ESP_LOGI(TAG, "Unlocked: %d", is_unlocked);
        }
    }
}

void log_mqtt(const std::string& msg)
{
    const auto topic = format("hal9k/acs/log/%s", get_identifier().c_str());
    const auto msg_id = esp_mqtt_client_enqueue(client, topic.c_str(),
                                                msg.c_str(), 0, 1, 0, true);
    ESP_LOGI(TAG, "Q %d", msg_id);
}

void set_mqtt_status(const std::string& subtopic,
                     const char* data)
{
    const auto topic = format("hal9k/acs/status/%s/%s",
                              get_identifier().c_str(),
                              subtopic.c_str());
    const auto msg_id = esp_mqtt_client_enqueue(client, topic.c_str(),
                                                data,
                                                0, 1, 1, true);
    ESP_LOGI(TAG, "Q %d", msg_id);
}

void start_mqtt(const std::string& mqtt_address)
{
    std::string mqtt_url = std::string("mqtt://") + mqtt_address;
    esp_mqtt_client_config_t mqtt_cfg = {
    };
    ESP_LOGI(TAG, "URL %s", mqtt_url.c_str());
    mqtt_cfg.broker.address.uri = mqtt_url.c_str();
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client,
                                   static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
