CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

SET(PROJECT frontend)

PROJECT(${PROJECT})

INCLUDE_DIRECTORIES(. ../../include/)

SET(SRCS
    buttons.cpp
    cardcache.cpp
    cardreader.cpp
    controller.cpp
    gateway.cpp
    display.cpp
    lock.cpp
    logger.cpp
    main.cpp
    serial.cpp
    serialib.cpp
    slack.cpp
    test.cpp
    util.cpp
)

ADD_EXECUTABLE(${PROJECT} ${SRCS})

SET_PROPERTY(TARGET ${PROJECT} PROPERTY CXX_STANDARD 20)

IF(${CMAKE_C_COMPILER} MATCHES arm)
    TARGET_LINK_LIBRARIES(${PROJECT}
        fmt
        restclient-cpp
        nghttp2
        unistring
        psl
        gssapi_krb5
        krb5
        krb5support
        k5crypto
        com_err
        sasl2
        keyutils
        nettle
        hogweed
        p11-kit
        tasn1
        ldap_r
        lber
        z
        gnutls
        gcrypt
        shishi
        idn2
        curl
        crypto
        ssl
        ssh2
        brotlicommon
        brotlidec
        rtmp
        gmp
        gss
        gpg-error
        ffi
        idn
        pthread
        atomic
        boost_program_options
    )
ELSE()
    TARGET_LINK_LIBRARIES(${PROJECT}
        fmt
        restclient-cpp
        z
        curl
        crypto
        ssl
        boost_program_options
    )
ENDIF()

ADD_DEFINITIONS(-Wno-psabi)

IF(${CMAKE_C_COMPILER} MATCHES arm)
    MESSAGE(STATUS "HOST $ENV{TARGET_HOST}:$ENV{TARGET_PORT}")
    ADD_CUSTOM_COMMAND(TARGET ${PROJECT} POST_BUILD COMMAND scp -P $$TARGET_PORT $<TARGET_FILE:${PROJECT}> $$TARGET_HOST:)
ENDIF()
