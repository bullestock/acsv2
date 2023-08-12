
idf_component_register(SRCS "TFT_eSPI.cpp" esp32-hal-gpio.c esp32-hal-spi.c esp32-hal-misc.c esp32-hal-matrix.c esp32-hal-cpu.c SPI.cpp
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES esp_timer driver spiffs)
