#pragma once

#include "defs.h"

#include <string>
#include <vector>

#include "esp_system.h"

esp_err_t connect(const wifi_creds_t& creds);
