#include "nvs.h"

#include "defs.h"

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "nvs_flash.h"

static char gateway_token[80];
static char slack_token[80];
static wifi_creds_t wifi_creds;
static uint8_t relay1_state;
static uint8_t relay2_state;

void clear_wifi_credentials()
{
    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_str(my_handle, WIFI_KEY, ""));
    nvs_close(my_handle);
    wifi_creds.clear();
}

void add_wifi_credentials(const char* ssid, const char* password)
{
    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    std::string creds;
    char buf[256];
    auto size = sizeof(buf);
    if (nvs_get_str(my_handle, WIFI_KEY, buf, &size) == ESP_OK)
        creds = std::string(buf);
    creds += std::string(ssid) + std::string(":") + std::string(password) + std::string(":");
    ESP_ERROR_CHECK(nvs_set_str(my_handle, WIFI_KEY, creds.c_str()));
    nvs_close(my_handle);
}

void set_gateway_token(const char* token)
{
    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_str(my_handle, GATEWAY_TOKEN_KEY, token));
    nvs_close(my_handle);
}

void set_slack_token(const char* token)
{
    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_str(my_handle, SLACK_TOKEN_KEY, token));
    nvs_close(my_handle);
}

void set_relay1_state(bool on)
{
    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_u8(my_handle, RELAY1_KEY, on));
    nvs_close(my_handle);
    relay1_state = on;
}

void set_relay2_state(bool on)
{
    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_u8(my_handle, RELAY2_KEY, on));
    nvs_close(my_handle);
    relay2_state = on;
}

bool get_nvs_string(nvs_handle my_handle, const char* key, char* buf, size_t buf_size)
{
    auto err = nvs_get_str(my_handle, key, buf, &buf_size);
    switch (err)
    {
    case ESP_OK:
        return true;
    case ESP_ERR_NVS_NOT_FOUND:
        printf("%s: not found\n", key);
        break;
    default:
        printf("%s: NVS error %d\n", key, err);
        break;
    }
    return false;
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

bool get_relay1_state()
{
    return relay1_state;
}

bool get_relay2_state()
{
    return relay2_state;
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
    if (get_nvs_string(my_handle, WIFI_KEY, buf, sizeof(buf)))
        wifi_creds = parse_wifi_credentials(buf);
    if (!get_nvs_string(my_handle, GATEWAY_TOKEN_KEY, gateway_token, sizeof(gateway_token)))
        gateway_token[0] = 0;
    if (!get_nvs_string(my_handle, SLACK_TOKEN_KEY, slack_token, sizeof(slack_token)))
        slack_token[0] = 0;
    if (!nvs_get_u8(my_handle, RELAY1_KEY, &relay1_state))
        relay1_state = false;
    if (!nvs_get_u8(my_handle, RELAY2_KEY, &relay2_state))
        relay2_state = false;
    nvs_close(my_handle);
}
