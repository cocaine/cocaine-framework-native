IF(APPLE)
    SET(CMAKE_MACOSX_RPATH ON)
ENDIF()

SET(Boost_USE_MULTITHREADED ON)
SET(BOOST_COMPONENTS
    system
    thread
)

FIND_PACKAGE(Boost COMPONENTS ${BOOST_COMPONENTS} REQUIRED)

SET(PROJECT cocaine-framework-cpp)

SET(HEADERS
    ${CMAKE_SOURCE_DIR}/include/cocaine/framework/connection
    ${CMAKE_SOURCE_DIR}/include/cocaine/framework/service
)

SET(SOURCES
    connection
    service
)

PROJECT(${PROJECT})

INCLUDE_DIRECTORIES(
    ${CMAKE_SOURCE_DIR}/include/cocaine/framework
    SYSTEM ${CMAKE_SOURCE_DIR}/../cocaine-core/cocaine-core/include
    ${CMAKE_SOURCE_DIR}/../cocaine-core/cocaine-core/foreign/asio/asio/include
    ${CMAKE_SOURCE_DIR}/../cocaine-core/cocaine-core/foreign/blackhole/src
)
LINK_DIRECTORIES(${CMAKE_SOURCE_DIR}/../cocaine-core/cocaine-core-build/)

ADD_LIBRARY(${PROJECT} SHARED
    ${HEADERS}
    ${SOURCES}
)

TARGET_LINK_LIBRARIES(${PROJECT}
    ${Boost_LIBRARIES}
    cocaine-core
    msgpack
)

SET_TARGET_PROPERTIES(${PROJECT} PROPERTIES
    COMPILE_FLAGS "-std=c++0x -Wall -pedantic -Wextra -Winit-self -Wconversion -Wno-sign-conversion -Weffc++ -Wold-style-cast -Woverloaded-virtual -Wctor-dtor-privacy -Wnon-virtual-dtor"
)

ADD_DEFINITIONS(-std=c++11)
