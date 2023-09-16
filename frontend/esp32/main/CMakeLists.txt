idf_component_register(SRCS
                       main.cpp
                       connect.cpp
                       console.cpp
                       controller.cpp
                       display.cpp
                       format.cpp
                       gateway.cpp
                       logger.cpp
                       hw.cpp
                       nvs.cpp
                       rs485.cpp
                       #slack.cpp
                       util.cpp
                       INCLUDE_DIRS "." "../../../include"
                       EMBED_TXTFILES howsmyssl_com_root_cert.pem gwtoken
)

#add_definitions(-DSIMULATE)
