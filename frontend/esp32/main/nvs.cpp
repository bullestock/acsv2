#include "nvs.h"

#include "defs.h"

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "nvs_flash.h"

static char identifier[20];
static char acs_token[80];
static char gateway_token[80];
static char slack_token[80];
static wifi_creds_t wifi_creds;
static char foreninglet_username[40];
static char foreninglet_password[40];
static uint8_t beta_program_active;

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

void set_identifier(const char* identifier)
{
    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_str(my_handle, IDENTIFIER_KEY, identifier));
    nvs_close(my_handle);
}

void set_descriptor(const char* descriptor)
{
    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_str(my_handle, DESCRIPTOR_KEY, descriptor));
    nvs_close(my_handle);
}

void set_acs_token(const char* token)
{
    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_str(my_handle, ACS_TOKEN_KEY, token));
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

void set_foreninglet_username(const char* user)
{
    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_str(my_handle, FL_USER_KEY, user));
    nvs_close(my_handle);
}

void set_foreninglet_password(const char* pass)
{
    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_str(my_handle, FL_PASS_KEY, pass));
    nvs_close(my_handle);
}

void set_beta_program_active(bool active)
{
    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    ESP_ERROR_CHECK(nvs_set_u8(my_handle, BETA_ACTIVE_KEY, active));
    nvs_close(my_handle);
    beta_program_active = active;
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

std::string get_acs_token()
{
    return acs_token;
}

std::string get_gateway_token()
{
    return gateway_token;
}

std::string get_identifier()
{
    if (identifier[0])
        return identifier;
    return "[device identifier not set]";
}

std::string get_descriptor()
{
    if (descriptor[0])
        return descriptor;
    return get_identifier();
}

std::string get_slack_token()
{
    return slack_token;
}

wifi_creds_t get_wifi_creds()
{
    return wifi_creds;
}

std::string get_foreninglet_username()
{
    return foreninglet_username;
}

std::string get_foreninglet_password()
{
    return foreninglet_password;
}

bool get_beta_program_active()
{
    return beta_program_active;
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
    if (!get_nvs_string(my_handle, IDENTIFIER_KEY, identifier, sizeof(identifier)))
        identifier[0] = 0;
    if (get_nvs_string(my_handle, WIFI_KEY, buf, sizeof(buf)))
        wifi_creds = parse_wifi_credentials(buf);
    if (!get_nvs_string(my_handle, ACS_TOKEN_KEY, acs_token, sizeof(acs_token)))
        acs_token[0] = 0;
    if (!get_nvs_string(my_handle, GATEWAY_TOKEN_KEY, gateway_token, sizeof(gateway_token)))
        gateway_token[0] = 0;
    if (!get_nvs_string(my_handle, SLACK_TOKEN_KEY, slack_token, sizeof(slack_token)))
        slack_token[0] = 0;
    if (!get_nvs_string(my_handle, FL_USER_KEY, foreninglet_username, sizeof(foreninglet_username)))
        foreninglet_username[0] = 0;
    if (!get_nvs_string(my_handle, FL_PASS_KEY, foreninglet_password, sizeof(foreninglet_password)))
        foreninglet_password[0] = 0;
    if (!nvs_get_u8(my_handle, BETA_ACTIVE_KEY, &beta_program_active))
        beta_program_active = false;
    nvs_close(my_handle);
}
