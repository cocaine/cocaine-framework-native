CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/tst/cmake")

SET(CMAKE_BUILD_TYPE "Debug")

# Prepare testing infrastructure.
INCLUDE(DownloadGoogleTesting)

DOWNLOAD_GOOGLE_TESTING()

INCLUDE_DIRECTORIES(
    SYSTEM ${GTEST_INCLUDE_DIR}
    SYSTEM ${GMOCK_INCLUDE_DIR}
)

LINK_DIRECTORIES(${GTEST_BINARY_DIR} ${GMOCK_BINARY_DIR})

set(COCAINE_CORE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/../cocaine-core/cocaine-core)

INCLUDE_DIRECTORIES(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${COCAINE_CORE_SOURCE_DIR}/include
    ${COCAINE_CORE_SOURCE_DIR}/foreign/asio/asio/include
    ${COCAINE_CORE_SOURCE_DIR}/foreign/blackhole/src
)

SET(Boost_USE_MULTITHREADED ON)
FIND_PACKAGE(Boost 1.40 REQUIRED COMPONENTS system thread)

SET(PROJECT cocaine-framework-cpp-tests)

SET(SOURCES
    util/net

    # session
    # session_t
    service
    func/real/connector
    func/real/service
    func/stub/session
)

PROJECT(${PROJECT})

ADD_EXECUTABLE(${PROJECT} ${SOURCES})
ADD_DEPENDENCIES(${PROJECT} googlemock)

TARGET_LINK_LIBRARIES(${PROJECT}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_LIBRARIES}
    cocaine-framework-cpp
    gmock
    gtest
    gmock_main
)

ADD_DEFINITIONS(-std=c++0x)
