#include "esp_system.h"
#include "esp_event.h"

#include <string>

#include "esp_log.h"
#include "mqtt_client.h"

#include "defs.h"

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
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;

    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void log_mqtt(const std::string& msg)
{
    const auto msg_id = esp_mqtt_client_enqueue(client, "/hal9k/acs/log/XXX",
                                                msg.c_str(), 0, 1, 0, true);
    ESP_LOGI(TAG, "enqueued, msg_id=%d", msg_id);
}

void start_mqtt(const std::string& mqtt_address)
{
    std::string mqtt_url = std::string("mqtt:") + mqtt_address;
    esp_mqtt_client_config_t mqtt_cfg = {
    };
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
