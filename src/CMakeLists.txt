IF(APPLE)
    SET(CMAKE_MACOSX_RPATH ON)
ENDIF()

SET(Boost_USE_MULTITHREADED ON)
FIND_PACKAGE(Boost 1.40 REQUIRED COMPONENTS system thread program_options)

SET(PROJECT cocaine-framework-cpp)

SET(COCAINE_FRAMEWORK_INCLUDES_DIR ${CMAKE_SOURCE_DIR}/include/cocaine/framework)

SET(SOURCES
    # connection ## Currently disabled
    net
    decoder
    error
    log
    manager
    scheduler
    resolver
    sender
    session
    service
    receiver
    worker.cpp
    worker/options
    worker/session

    util/future/error
)

PROJECT(${PROJECT})

set(COCAINE_CORE_DIR ${CMAKE_SOURCE_DIR}/../cocaine-core/cocaine-core/)

INCLUDE_DIRECTORIES(
    ${CMAKE_SOURCE_DIR}/include
    SYSTEM ${COCAINE_CORE_DIR}/include
    ${COCAINE_CORE_DIR}/foreign/blackhole/src
    ${COCAINE_CORE_DIR}/foreign/asio/asio/include
)

ADD_LIBRARY(${PROJECT} SHARED ${SOURCES})

TARGET_LINK_LIBRARIES(${PROJECT}
    ${Boost_LIBRARIES}
    msgpack
)

SET_TARGET_PROPERTIES(${PROJECT} PROPERTIES
    COMPILE_FLAGS "-Wall -pedantic -Wextra -Winit-self -Wconversion -Wno-sign-conversion -Wold-style-cast -Woverloaded-virtual -Wctor-dtor-privacy -Wnon-virtual-dtor"
)

ADD_DEFINITIONS(-std=c++0x)
