#pragma once

#include <string>
#include <vector>

#include "esp_system.h"

esp_err_t connect(const std::vector<std::pair<std::string, std::string>>& creds);
