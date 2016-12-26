cmake_minimum_required(VERSION 2.6)

include_directories(
    ${CMAKE_SOURCE_DIR}/include)

set(PROJECT echo)

set(SOURCES main)

project(${PROJECT})

add_executable(${PROJECT}
    ${SOURCES})

target_link_libraries(${PROJECT}
    cocaine-framework-native
    cocaine-io-util)

add_definitions(-std=c++0x)

file(COPY manifest.json DESTINATION .)
