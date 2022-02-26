#ifndef __LCD_LIB_H__
#define __LCD_LIB_H__

#include "fontx.h"

#ifdef __cplusplus
extern "C" {
#endif

// Controller-dependent function
void (*DrawPixel)(TFT_t * dev, uint16_t x, uint16_t y, uint16_t color);
void (*DrawMultiPixels)(TFT_t * dev, uint16_t x, uint16_t y, uint16_t size, uint16_t * colors);
void (*DrawFillRect)(TFT_t * dev, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
void (*DisplayOff)(TFT_t * dev);
void (*DisplayOn)(TFT_t * dev);
void (*InversionOff)(TFT_t * dev);
void (*InversionOn)(TFT_t * dev);
bool (*EnableScroll)(TFT_t * dev);
void (*SetScrollArea)(TFT_t * dev, uint16_t tfa, uint16_t vsa, uint16_t bfa);
void (*ResetScrollArea)(TFT_t * dev, uint16_t vsa);
void (*StartScroll)(TFT_t * dev, uint16_t vsp);

#ifdef __cplusplus
}
#endif

#endif
