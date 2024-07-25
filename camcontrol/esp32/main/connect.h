#pragma once

#include "defs.h"

#include <string>
#include <vector>

#include "esp_netif.h"
#include "esp_system.h"

bool connect(const wifi_creds_t& creds);
esp_err_t disconnect();

esp_ip4_addr_t get_ip_address();

