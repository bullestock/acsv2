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


static void mqtt_event_handler(void* handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void* event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    auto event = reinterpret_cast<esp_mqtt_event_handle_t>(event_data);
    switch ((esp_mqtt_event_id_t) event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        connected = true;
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        connected = false;
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR: %d", event->msg_id);
        break;

    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void log_mqtt(const std::string& msg)
{
    const auto topic = format("hal9k/acs/log/%s", get_identifier().c_str());
    const auto msg_id = esp_mqtt_client_enqueue(client, topic.c_str(),
                                                msg.c_str(), 0, 1, 0, true);
    ESP_LOGI(TAG, "enqueued, msg_id=%d", msg_id);
}

void set_mqtt_status(const std::string& subtopic,
                     const std::string& msg)
{
    time_t t = time(nullptr);
    struct tm tm;
    gmtime_r(&t, &tm);
    char buf[40];
    sprintf(buf, "%04d-%02d-%02dT%02d:%02d:%02d",
            1900+tm.tm_year, tm.tm_mon+1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
    auto payload = cJSON_CreateObject();
    cJSON_wrapper jw(payload);
    auto timestamp = cJSON_CreateString(buf);
    cJSON_AddItemToObject(payload, "timestamp", timestamp);
    auto message = cJSON_CreateString(msg.c_str());
    cJSON_AddItemToObject(payload, "message", message);
    char* data = cJSON_PrintUnformatted(payload);
    if (!data)
    {
        ESP_LOGE(TAG, "cJSON_Print() returned nullptr");
        return;
    }
    cJSON_Print_wrapper pw(data);

    const auto topic = format("hal9k/acs/status/%s/%s",
                              get_identifier().c_str(),
                              subtopic.c_str());
    const auto msg_id = esp_mqtt_client_enqueue(client, topic.c_str(),
                                                data,
                                                0, 1, 1, true);
    ESP_LOGI(TAG, "status enqueued, msg_id=%d", msg_id);
}

void start_mqtt(const std::string& mqtt_address)
{
    std::string mqtt_url = std::string("mqtt://") + mqtt_address;
    esp_mqtt_client_config_t mqtt_cfg = {
    };
    ESP_LOGI(TAG, "MQTT URL %s", mqtt_url.c_str());
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
