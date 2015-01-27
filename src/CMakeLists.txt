IF(APPLE)
    SET(CMAKE_MACOSX_RPATH ON)
ENDIF()

SET(CMAKE_BUILD_TYPE "Debug")

SET(Boost_USE_MULTITHREADED ON)
SET(BOOST_COMPONENTS
    system
    thread
)

FIND_PACKAGE(Boost COMPONENTS ${BOOST_COMPONENTS} REQUIRED)

SET(PROJECT cocaine-framework-cpp)

SET(COCAINE_FRAMEWORK_INCLUDES_DIR ${CMAKE_SOURCE_DIR}/include/cocaine/framework)

SET(HEADERS
    ${COCAINE_FRAMEWORK_INCLUDES_DIR}/common
    ${COCAINE_FRAMEWORK_INCLUDES_DIR}/config
    ${COCAINE_FRAMEWORK_INCLUDES_DIR}/garbage
    ${COCAINE_FRAMEWORK_INCLUDES_DIR}/future
    ${COCAINE_FRAMEWORK_INCLUDES_DIR}/connection
    ${COCAINE_FRAMEWORK_INCLUDES_DIR}/detail/log
    ${COCAINE_FRAMEWORK_INCLUDES_DIR}/service
    ${COCAINE_FRAMEWORK_INCLUDES_DIR}/session
    ${COCAINE_FRAMEWORK_INCLUDES_DIR}/util/future.hpp
    ${COCAINE_FRAMEWORK_INCLUDES_DIR}/util/future/error
    ${COCAINE_FRAMEWORK_INCLUDES_DIR}/util/future/future
    ${COCAINE_FRAMEWORK_INCLUDES_DIR}/util/future/future_detail
    ${COCAINE_FRAMEWORK_INCLUDES_DIR}/util/future/packaged_task
    ${COCAINE_FRAMEWORK_INCLUDES_DIR}/util/future/promise
    ${COCAINE_FRAMEWORK_INCLUDES_DIR}/util/future/shared_state
    ${COCAINE_FRAMEWORK_INCLUDES_DIR}/util/future/traits
    ${COCAINE_FRAMEWORK_INCLUDES_DIR}/util/future/utility
    ${COCAINE_FRAMEWORK_INCLUDES_DIR}/util/future/variant
)

SET(SOURCES
    connection
    log
    service
    session
    util/future/error
)

PROJECT(${PROJECT})

INCLUDE_DIRECTORIES(
    ${CMAKE_SOURCE_DIR}/include
    SYSTEM ${CMAKE_SOURCE_DIR}/../cocaine-core/cocaine-core/include
    ${CMAKE_SOURCE_DIR}/../cocaine-core/cocaine-core/foreign/asio/asio/include
    ${CMAKE_SOURCE_DIR}/../cocaine-core/cocaine-core/foreign/blackhole/src
)

ADD_LIBRARY(${PROJECT} SHARED
    ${HEADERS}
    ${SOURCES}
)

TARGET_LINK_LIBRARIES(${PROJECT}
    ${Boost_LIBRARIES}
    msgpack
)

SET_TARGET_PROPERTIES(${PROJECT} PROPERTIES
    COMPILE_FLAGS "-std=c++0x -Wall -pedantic -Wextra -Winit-self -Wconversion -Wno-sign-conversion -Wold-style-cast -Woverloaded-virtual -Wctor-dtor-privacy -Wnon-virtual-dtor"
)

ADD_DEFINITIONS(-std=c++11)
