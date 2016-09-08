list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/tst/cmake")

set(CMAKE_BUILD_TYPE "Release")

# Prepare testing infrastructure.
include(DownloadGoogleTesting)

download_google_testing()

include_directories(
    SYSTEM ${GTEST_INCLUDE_DIR}
    SYSTEM ${GMOCK_INCLUDE_DIR})

link_directories(${GTEST_BINARY_DIR} ${GMOCK_BINARY_DIR})

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    SYSTEM ${CMAKE_SOURCE_DIR}/foreign/asio/asio/include)

set(PROJECT cocaine-framework-tests)

set(SOURCES
    main
    util/net
    func/real/connector
# Temporary suppressed, because of Blackhole version on build farm.
    func/real/logging
    func/real/service
    func/stub/session
    func/manual/service
)

project(${PROJECT})

add_executable(${PROJECT} ${SOURCES})
add_dependencies(${PROJECT} googletest)
target_link_libraries(${PROJECT}
    cocaine-framework-native
    gmock
    gtest)

add_definitions(-std=c++0x)

# To be able to run load tests you should put a file named "load.cfg" in the current directory.
# This file contains each test name and its arguments separated by space.
# For example: load.service.echo 1000 echo ping
#              ^                 ^    ^    ^
#              test name         |    app  event
#                                iterations

add_executable(load
    load/main
    load/stats
    load/app/echo
    load/app/http
# Suppressed, because of echo service unavailability.
    #load/service/echo
    load/service/storage
    load/service/logging
)

add_dependencies(load googletest)

target_link_libraries(load
    cocaine-framework-native
    gmock
    gtest)
