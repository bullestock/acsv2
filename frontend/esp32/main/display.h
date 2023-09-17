#pragma once

#include <string>

#include <TFT_eSPI.h>

void init(TFT_eSPI& tft);

void set_status(TFT_eSPI& tft, const std::string& status,
                uint16_t colour = TFT_WHITE, bool large = false);

void add_progress(TFT_eSPI& tft, const std::string& status);

void show_message(TFT_eSPI& tft, const std::string& message, uint16_t colour = TFT_WHITE);
