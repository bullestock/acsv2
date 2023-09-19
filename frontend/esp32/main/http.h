#pragma once

#include "esp_http_client.h"
#include "esp_system.h"

esp_err_t http_event_handler(esp_http_client_event_t* evt);

extern int http_output_len;

extern const char howsmyssl_com_root_cert_pem_start[] asm("_binary_howsmyssl_com_root_cert_pem_start");
extern const char howsmyssl_com_root_cert_pem_end[]   asm("_binary_howsmyssl_com_root_cert_pem_end");
