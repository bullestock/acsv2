/**
 * Port of NeoPixelBus to run under ESP8266_RTOS
 * Origin repo: https://github.com/Makuna/NeoPixelBus
 * 
 * minimize modifications on the original NeoPixelBus implementation
 */

#pragma once

#include "sdkconfig.h"

#include <cstdint>
#include <cmath>
#include <cstring>
#include <sys/time.h>

// disbale SPI code branch
#define __AVR_ATtiny85__

// not needed any more
#define PGM_P  const char*
#define PROGMEM

// used in HtmlColor.h
#define strncpy_P   std::strncpy
#define strlen_P    std::strlen

// used in NeoEase.h
#define HALF_PI  M_PI_2
#define PI       M_PI

#ifdef __cplusplus
// used in HtmlColor.h
#include <string>
typedef std::string  String;

extern "C" {
#endif // __cplusplus

inline uint8_t pgm_read_byte(const void* p) {
    return *(const uint8_t*)(p);
}

inline uint16_t pgm_read_dword(const void* p) {
    return *(const uint16_t*)(p);
}

inline unsigned long micros() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return 1000000 * tv.tv_sec + tv.tv_usec;
}

inline unsigned long millis() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000L) + (tv.tv_usec / 1000L);
}

#ifdef __cplusplus
}
#endif
