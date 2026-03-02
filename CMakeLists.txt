cmake_minimum_required(VERSION 3.14)
project(wsocket_io_c VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LWS REQUIRED libwebsockets)
pkg_check_modules(CURL REQUIRED libcurl)

add_library(wsocket_io
    src/wsocket.c
    src/cJSON.c
)

target_include_directories(wsocket_io
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${LWS_INCLUDE_DIRS}
        ${CURL_INCLUDE_DIRS}
)

target_link_libraries(wsocket_io
    PRIVATE
        ${LWS_LIBRARIES}
        ${CURL_LIBRARIES}
        pthread
)

install(TARGETS wsocket_io LIBRARY DESTINATION lib)
install(FILES include/wsocket.h DESTINATION include)
