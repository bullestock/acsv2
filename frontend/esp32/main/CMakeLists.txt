idf_component_register(SRCS
                       main.cpp
                       buttons.cpp
                       cardcache.cpp
                       cardreader.cpp
                       connect.cpp
                       console.cpp
                       controller.cpp
                       display.cpp
                       foreninglet.cpp
                       format.cpp
                       http.cpp
                       hw.cpp
                       mqtt.cpp
                       nvs.cpp
                       otafwu.cpp
                       rs485.cpp
                       sntp.cpp
                       util.cpp
                       REQUIRES app_update console esp_app_format esp_driver_gpio esp_driver_i2c esp_driver_ledc
                       esp_driver_spi esp_driver_uart esp_http_client esp_wifi mbedtls nvs_flash TFT_eSPI
                       INCLUDE_DIRS "."
)

#add_definitions(-DSIMULATE)

# Local Variables:
# compile-command: "cd .. && idf.py build"
# End:
