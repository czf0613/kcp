CMAKE_MINIMUM_REQUIRED(VERSION 3.10)

project(kcp LANGUAGES C)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

add_library(kcp_static STATIC ikcp.c)
set_target_properties(kcp_static PROPERTIES
    OUTPUT_NAME kcp
)

add_library(kcp_shared SHARED ikcp.c)
set_target_properties(kcp_shared PROPERTIES
    OUTPUT_NAME kcp
)

if(UNIX)
    add_executable(kcp_test main.c)
    target_link_libraries(kcp_test kcp_static)
endif()