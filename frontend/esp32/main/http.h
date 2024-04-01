#pragma once

#include "esp_http_client.h"
#include "esp_system.h"

esp_err_t http_event_handler(esp_http_client_event_t* evt);

struct Http_data
{
    /// Output buffer
    char* buffer = nullptr;

    /// Maximum size of output buffer
    int max_output = 0;

    /// Current size of output buffer
    int output_len = 0;
};

extern const char howsmyssl_com_root_cert_pem_start[] asm("_binary_howsmyssl_com_root_cert_pem_start");
extern const char howsmyssl_com_root_cert_pem_end[]   asm("_binary_howsmyssl_com_root_cert_pem_end");

/// RAII class to ensure calling esp_http_client_cleanup() on a handle.
class Http_client_wrapper
{
public:
    Http_client_wrapper(esp_http_client_handle_t& handle);

    ~Http_client_wrapper();

private:
    esp_http_client_handle_t handle;
};
