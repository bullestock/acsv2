idf_component_register(
    SRCS connect.cpp gateway.cpp gpio.cpp led.cpp main.cpp
    INCLUDE_DIRS ""
)

set(COMPONENT_EMBED_TXTFILES server_root_cert.pem)
