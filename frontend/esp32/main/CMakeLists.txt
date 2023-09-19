idf_component_register(SRCS
                       main.cpp
                       cardcache.cpp
                       connect.cpp
                       console.cpp
                       controller.cpp
                       display.cpp
                       format.cpp
                       gateway.cpp
                       logger.cpp
                       http.cpp
                       hw.cpp
                       nvs.cpp
                       rs485.cpp
                       slack.cpp
                       sntp.cpp
                       util.cpp
                       INCLUDE_DIRS "." "../../../include"
                       EMBED_TXTFILES howsmyssl_com_root_cert.pem
)

#add_definitions(-DSIMULATE)
