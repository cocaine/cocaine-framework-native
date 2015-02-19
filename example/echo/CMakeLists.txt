CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

INCLUDE_DIRECTORIES(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/../cocaine-core/cocaine-core/include
    ${CMAKE_SOURCE_DIR}/../cocaine-core/cocaine-core/foreign/asio/asio/include
    ${CMAKE_SOURCE_DIR}/../cocaine-core/cocaine-core/foreign/blackhole/src
)

SET(Boost_USE_MULTITHREADED ON)
FIND_PACKAGE(Boost 1.40 REQUIRED COMPONENTS system thread)

SET(PROJECT echo)

SET(HEADERS
)

SET(SOURCES
    main
)

PROJECT(${PROJECT})

ADD_EXECUTABLE(${PROJECT} ${SOURCES})
ADD_DEPENDENCIES(${PROJECT} googlemock)

TARGET_LINK_LIBRARIES(${PROJECT}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_LIBRARIES}
    cocaine-framework-cpp
)

ADD_DEFINITIONS(-std=c++0x)
