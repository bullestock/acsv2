/* Common functions for protocol examples, to establish Wi-Fi or Ethernet connection.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include "connect.h"
#include "defines.h"

#include <string.h>

#include "sdkconfig.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"

static xSemaphoreHandle s_semph_get_ip_addrs;
static esp_netif_t* s_esp_netif = NULL;

static esp_netif_t* wifi_start(const std::pair<std::string, std::string>& creds);
static void wifi_stop();

static esp_ip4_addr_t s_ip_addr;

static void on_got_ip(void* arg, esp_event_base_t event_base,
                      int32_t event_id, void* event_data)
{
    printf("Got IP\n");
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    memcpy(&s_ip_addr, &event->ip_info.ip, sizeof(s_ip_addr));
    xSemaphoreGive(s_semph_get_ip_addrs);
}

esp_err_t connect(const std::vector<std::pair<std::string, std::string>>& creds)
{
    if (s_semph_get_ip_addrs != NULL)
        return ESP_ERR_INVALID_STATE;
    s_semph_get_ip_addrs = xSemaphoreCreateCounting(1, 0);
    int index = 0;
    s_esp_netif = wifi_start(creds[index]);
    ESP_ERROR_CHECK(esp_register_shutdown_handler(&wifi_stop));
    printf("Waiting for IP(s)\n");
    while (!xSemaphoreTake(s_semph_get_ip_addrs, 10000/portTICK_PERIOD_MS))
    {
        printf("Trying next SSID\n");
        wifi_stop();
        ++index;
        if (index >= creds.size())
            return ESP_ERR_WIFI_NOT_STARTED;
        s_esp_netif = wifi_start(creds[index]);
    }
    printf("Got IP(s)\n");
    // iterate over active interfaces, and print out IPs of "our" netifs
    esp_netif_t* netif = nullptr;
    for (int i = 0; i < esp_netif_get_nr_of_ifs(); ++i) {
        netif = esp_netif_next(netif);
        printf("Connected to %s\n", esp_netif_get_desc(netif));
        esp_netif_ip_info_t ip;
        ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &ip));

        printf("- IPv4 address: " IPSTR "\n", IP2STR(&ip.ip));
    }
    return ESP_OK;
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
    printf("on_wifi_disconnect: %d\n", event_id);
    printf("Wi-Fi disconnected, trying to reconnect...\n");
    esp_err_t err = esp_wifi_connect();
    if (err == ESP_ERR_WIFI_NOT_STARTED)
        return;
    ESP_ERROR_CHECK(err);
}

static esp_netif_t* wifi_start(const std::pair<std::string, std::string>& creds)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    esp_netif_config.route_prio = 128;
    esp_netif_t* netif = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
    esp_wifi_set_default_wifi_sta_handlers();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_wifi_disconnect, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    strncpy((char*) wifi_config.sta.ssid, creds.first.c_str(), sizeof(wifi_config.sta.ssid));
    strncpy((char*) wifi_config.sta.password, creds.second.c_str(), sizeof(wifi_config.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    printf("Connecting to %s\n", wifi_config.sta.ssid);
    esp_wifi_connect();
    return netif;
}

esp_netif_t* get_netif_from_desc(const char* desc)
{
    esp_netif_t* netif = NULL;
    while ((netif = esp_netif_next(netif)) != NULL)
        if (strcmp(esp_netif_get_desc(netif), desc) == 0)
            return netif;
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
