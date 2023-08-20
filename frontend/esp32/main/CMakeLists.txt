idf_component_register(SRCS
                       main.cpp
                       connect.cpp
                       console.cpp
                       format.cpp
                       gateway.cpp
                       hw.cpp
                       rs485.cpp
                       util.cpp
                       INCLUDE_DIRS "."
                       EMBED_TXTFILES howsmyssl_com_root_cert.pem gwtoken
)

#add_definitions(-DSIMULATE)
