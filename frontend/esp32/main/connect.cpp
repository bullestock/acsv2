/* Common functions for protocol examples, to establish Wi-Fi or Ethernet connection.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include "connect.h"
#include "defs.h"

#include <string.h>

#include "sdkconfig.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"

static SemaphoreHandle_t s_semph_get_ip_addrs;
static esp_netif_t* s_esp_netif = NULL;

static esp_netif_t* wifi_start(const std::string& ssid, const std::string& password);
static void wifi_stop();

/**
 * @brief Checks the netif description if it contains specified prefix.
 * All netifs created withing common connect component are prefixed with the module TAG,
 * so it returns true if the specified netif is owned by this module
 */
static bool is_our_netif(const char* prefix, esp_netif_t* netif)
{
    return strncmp(prefix, esp_netif_get_desc(netif), strlen(prefix) - 1) == 0;
}

static esp_ip4_addr_t s_ip_addr;

esp_ip4_addr_t get_ip_address()
{
    return s_ip_addr;
}

static void on_got_ip(void* arg, esp_event_base_t event_base,
                      int32_t event_id, void* event_data)
{
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    if (!is_our_netif(TAG, event->esp_netif)) {
        ESP_LOGW(TAG, "Got IPv4 from another interface \"%s\": ignored", esp_netif_get_desc(event->esp_netif));
        return;
    }
    ESP_LOGI(TAG, "Got IPv4 event: Interface \"%s\" address: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
    memcpy(&s_ip_addr, &event->ip_info.ip, sizeof(s_ip_addr));
    xSemaphoreGive(s_semph_get_ip_addrs);
}

bool connect(const wifi_creds_t& creds)
{
    if (s_semph_get_ip_addrs != NULL)
        return ESP_ERR_INVALID_STATE;
    s_semph_get_ip_addrs = xSemaphoreCreateCounting(1, 0);
    int index = 0;
    s_esp_netif = wifi_start(creds[index].first, creds[index].second);
    ESP_ERROR_CHECK(esp_register_shutdown_handler(&wifi_stop));
    ESP_LOGI(TAG, "Waiting for IP");
    while (!xSemaphoreTake(s_semph_get_ip_addrs, 10000/portTICK_PERIOD_MS))
    {
        ESP_LOGI(TAG, "Failed to connect");
        wifi_stop();
        ++index;
        if (index >= creds.size())
            return false;
        ESP_LOGI(TAG, "Trying next SSID");
        s_esp_netif = wifi_start(creds[index].first, creds[index].second);
    }
    ESP_LOGI(TAG, "Got IP(s)");
    // iterate over active interfaces, and print out IPs of "our" netifs
    esp_netif_t* netif = nullptr;
    for (int i = 0; i < esp_netif_get_nr_of_ifs(); ++i) {
        netif = esp_netif_next(netif);
        if (is_our_netif(TAG, netif)) {
            ESP_LOGI(TAG, "Connected to %s", esp_netif_get_desc(netif));
            esp_netif_ip_info_t ip;
            ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &ip));

            ESP_LOGI(TAG, "- IPv4 address: " IPSTR, IP2STR(&ip.ip));
        }
    }
    return true;
}

esp_err_t disconnect()
{
    if (s_semph_get_ip_addrs == NULL)
        return ESP_ERR_INVALID_STATE;
    vSemaphoreDelete(s_semph_get_ip_addrs);
    s_semph_get_ip_addrs = NULL;
    wifi_stop();
    ESP_ERROR_CHECK(esp_unregister_shutdown_handler(&wifi_stop));
    return ESP_OK;
}

static void on_wifi_disconnect(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    ESP_LOGI(TAG, "on_wifi_disconnect: %d", (int) event_id);
    ESP_LOGI(TAG, "Wi-Fi disconnected, trying to reconnect...");
    esp_err_t err = esp_wifi_connect();
    if (err == ESP_ERR_WIFI_NOT_STARTED)
        return;
    ESP_ERROR_CHECK(err);
}

static esp_netif_t* wifi_start(const std::string& ssid, const std::string& password)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    // Prefix the interface description with the module TAG
    // Warning: the interface desc is used in tests to capture actual connection details (IP, gw, mask)
    char* desc;
    asprintf(&desc, "%s: %s", TAG, esp_netif_config.if_desc);
    esp_netif_config.if_desc = desc;
    esp_netif_config.route_prio = 128;
    esp_netif_t* netif = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
    free(desc);
    esp_wifi_set_default_wifi_sta_handlers();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    strncpy((char*) wifi_config.sta.ssid, ssid.c_str(), sizeof(wifi_config.sta.ssid));
    strncpy((char*) wifi_config.sta.password, password.c_str(), sizeof(wifi_config.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Connecting to %s", wifi_config.sta.ssid);
    esp_wifi_connect();
    return netif;
}

esp_netif_t* get_netif_from_desc(const char* desc)
{
    char* expected_desc;
    asprintf(&expected_desc, "%s: %s", TAG, desc);
    esp_netif_t* netif = NULL;
    while ((netif = esp_netif_next(netif)) != NULL)
        if (strcmp(esp_netif_get_desc(netif), expected_desc) == 0)
        {
            free(expected_desc);
            return netif;
        }
    free(expected_desc);
    return netif;
}

static void wifi_stop()
{
    esp_netif_t* wifi_netif = get_netif_from_desc("sta");
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip));
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT)
        return;
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(wifi_netif));
    esp_netif_destroy(wifi_netif);
    s_esp_netif = NULL;
}

esp_netif_t* get_netif()
{
    return s_esp_netif;
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
