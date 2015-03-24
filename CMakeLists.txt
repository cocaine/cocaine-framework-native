cmake_minimum_required(VERSION 2.6)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(WITH_TRACE "Build the library with trace logging" OFF)

if(WITH_TRACE)
    set(COCAINE_FRAMEWORK_HAS_INTERNAL_TRACING 1)
endif()

configure_file(
    "${CMAKE_SOURCE_DIR}/config.hpp.in"
    "${CMAKE_SOURCE_DIR}/include/cocaine/framework/config.hpp")

add_subdirectory(src)
add_subdirectory(example/echo)
add_subdirectory(example/http)
add_subdirectory(tst)
