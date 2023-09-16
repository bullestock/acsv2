#include "nvs.h"

#include "defs.h"

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "nvs_flash.h"

static char gateway_token[80];
static char slack_token[80];
static wifi_creds_t wifi_creds;

// Reboots if key not found
void get_nvs_string(nvs_handle my_handle, const char* key, char* buf, size_t buf_size)
{
    auto err = nvs_get_str(my_handle, key, buf, &buf_size);
    switch (err)
    {
    case ESP_OK:
        return;
    case ESP_ERR_NVS_NOT_FOUND:
        printf("%s: not found\n", key);
        break;
    default:
        printf("%s: NVS error %d\n", key, err);
        break;
    }
    printf("Restart in 10 seconds\n");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    printf("Restart in 5 seconds\n");
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    esp_restart();
}

std::vector<std::pair<std::string, std::string>> parse_wifi_credentials(char* buf)
{
    std::vector<std::pair<std::string, std::string>> v;
    bool is_ssid = true;
    std::string ssid;
    char* p = buf;
    while (1)
    {
        char* token = strsep(&p, ":");
        if (!token)
            break;
        if (is_ssid)
            ssid = std::string(token);
        else
            v.push_back(std::make_pair(ssid, std::string(token)));
        is_ssid = !is_ssid;
    }
    return v;
}

std::string get_gateway_token()
{
    return gateway_token;
}

std::string get_slack_token()
{
    return slack_token;
}

wifi_creds_t get_wifi_creds()
{
    return wifi_creds;
}

void init_nvs()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    char buf[256];
    get_nvs_string(my_handle, WIFI_KEY, buf, sizeof(buf));
    wifi_creds = parse_wifi_credentials(buf);
    get_nvs_string(my_handle, GATEWAY_TOKEN_KEY, gateway_token, sizeof(gateway_token));
    get_nvs_string(my_handle, SLACK_TOKEN_KEY, slack_token, sizeof(slack_token));
    nvs_close(my_handle);
}
