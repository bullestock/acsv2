#pragma once

#include <TFT_eSPI.h>

void init(TFT_eSPI& tft);

void set_status(TFT_eSPI& tft, const std::string& status, uint16_t colour = TFT_WHITE);
