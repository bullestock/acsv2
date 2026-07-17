#include "nvs.h"

#include "defs.h"

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "nvs_flash.h"

static char mqtt_address[80];
static char identifier[20];
static char acs_token[80];
static char slack_token[80];
static uint8_t private_key[SIGNING_KEY_SIZE];
static wifi_creds_t wifi_creds;
static char foreninglet_username[40];
static char foreninglet_password[40];
uint8_t is_main = 0;

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

void set_mqtt_address(const char* address)
{
    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_str(my_handle, MQTT_ADDRESS_KEY, address));
    nvs_close(my_handle);
}

void set_identifier(const char* identifier)
{
    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_str(my_handle, IDENTIFIER_KEY, identifier));
    nvs_close(my_handle);
}

void set_acs_token(const char* token)
{
    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_str(my_handle, ACS_TOKEN_KEY, token));
    nvs_close(my_handle);
}

void set_slack_token(const char* token)
{
    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_str(my_handle, SLACK_TOKEN_KEY, token));
    nvs_close(my_handle);
}

void set_private_key(const uint8_t* key)
{
    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_blob(my_handle, PRIVKEY_KEY, key, SIGNING_KEY_SIZE));
    nvs_close(my_handle);
}

void set_is_main(bool is_main)
{
    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_u8(my_handle, ISMAIN_KEY, is_main));
    nvs_close(my_handle);
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

std::string get_mqtt_address()
{
    return mqtt_address;
}

std::string get_acs_token()
{
    return acs_token;
}

std::string get_identifier()
{
    if (identifier[0])
        return identifier;
    return "[device identifier not set]";
}

std::string get_slack_token()
{
    return slack_token;
}

const uint8_t* get_private_key()
{
    return private_key;
}

wifi_creds_t get_wifi_creds()
{
    return wifi_creds;
}

bool get_is_main()
{
    return is_main;
}

std::string get_foreninglet_username()
{
    return foreninglet_username;
}

std::string get_foreninglet_password()
{
    return foreninglet_password;
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
    if (!get_nvs_string(my_handle, IDENTIFIER_KEY, identifier, sizeof(identifier)))
        identifier[0] = 0;
    char buf[256];
    if (get_nvs_string(my_handle, WIFI_KEY, buf, sizeof(buf)))
        wifi_creds = parse_wifi_credentials(buf);
    if (!get_nvs_string(my_handle, MQTT_ADDRESS_KEY, mqtt_address, sizeof(mqtt_address)))
        strcpy(mqtt_address, "imqtt.hal9k.dk");
    if (!get_nvs_string(my_handle, ACS_TOKEN_KEY, acs_token, sizeof(acs_token)))
        acs_token[0] = 0;
    if (!get_nvs_string(my_handle, SLACK_TOKEN_KEY, slack_token, sizeof(slack_token)))
        slack_token[0] = 0;
    size_t private_key_len = SIGNING_KEY_SIZE;
    auto err = nvs_get_blob(my_handle, PRIVKEY_KEY, private_key, &private_key_len);
    if (err != ESP_OK || private_key_len != SIGNING_KEY_SIZE)
        private_key_len = 0;
    if (nvs_get_u8(my_handle, ISMAIN_KEY, &is_main) != ESP_OK)
        is_main = false;
    nvs_close(my_handle);
}

// Local Variables:
// compile-command: "cd .. && idf.py build"
// End:
