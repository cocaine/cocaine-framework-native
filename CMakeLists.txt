cmake_minimum_required(VERSION 2.6)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(CF_USE_INTERNAL_LOGGING "Build the library with trace logging" OFF)

configure_file(
    "${CMAKE_SOURCE_DIR}/config.hpp.in"
    "${CMAKE_SOURCE_DIR}/include/cocaine/framework/config.hpp")

add_subdirectory(src)
add_subdirectory(example/echo)
add_subdirectory(example/http)
add_subdirectory(tst)
