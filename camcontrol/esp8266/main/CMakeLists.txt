idf_component_register(
    SRCS connect.cpp eventhandler.cpp gateway.cpp gpio.cpp led.cpp main.cpp
    INCLUDE_DIRS "."
    EMBED_TXTFILES howsmyssl_com_root_cert.pem gwtoken
)
