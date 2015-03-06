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

set(PROJECT cocaine-framework-cpp-tests)

set(SOURCES
    main
    util/net
    service
    func/real/connector
    func/real/service
    func/stub/session
#    load/service
)

project(${PROJECT})

add_executable(${PROJECT} ${SOURCES})
add_dependencies(${PROJECT} googlemock)
target_link_libraries(${PROJECT}
    cocaine-framework-cpp
    gmock
    gtest)

add_definitions(-std=c++0x)
