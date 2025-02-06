cmake_minimum_required(VERSION 3.6)

project(ias-client)

#---------------------------------------------------------------------------------

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(libtooldir ${CMAKE_CURRENT_SOURCE_DIR}/../ias-libtool/)

include_directories(${libtooldir})

if (DEFINED ANDROID_ABI)
    message("Building library for android client")

    include_directories(${CMAKE_CURRENT_BINARY_DIR}/../ias-libtool/boringssl/src/boringssl/include)

    add_library(ias-client SHARED
            ias_client.cpp
            callback.cpp
            timer.cpp
            measurement.cpp
            android_connector.cpp
            tests/ip.cpp
            tests/ping.cpp
            tests/download.cpp
            tests/upload.cpp
            tests/upload_sender.cpp
    )

    TARGET_LINK_LIBRARIES(ias-client
            ${CMAKE_THREAD_LIBS_INIT}
            iastool
            android
            log
    )
else ()
    message("Building executable")

    IF (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        include_directories(/opt/homebrew/opt/openssl@3/include)
        link_directories(/opt/homebrew/opt/openssl@3/lib)
    endif ()

    add_executable(ias-client
            ias_client.cpp
            callback.cpp
            timer.cpp
            measurement.cpp
            tests/ip.cpp
            tests/ping.cpp
            tests/download.cpp
            tests/upload.cpp
            tests/upload_sender.cpp
    )

    TARGET_LINK_LIBRARIES(ias-client
            ${CMAKE_THREAD_LIBS_INIT}
            iastool
            ssl
    )
endif ()

INCLUDE(CPack)