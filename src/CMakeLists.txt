IF(APPLE)
    SET(CMAKE_MACOSX_RPATH ON)
ENDIF()

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
    ${CMAKE_SOURCE_DIR}/../cocaine-core/cocaine-core/include
    ${CMAKE_SOURCE_DIR}/../cocaine-core/cocaine-core/foreign/asio/asio/include
    ${CMAKE_SOURCE_DIR}/../cocaine-core/cocaine-core/foreign/blackhole/src
)
LINK_DIRECTORIES(${CMAKE_SOURCE_DIR}/../cocaine-core/cocaine-core-build/)

ADD_LIBRARY(${PROJECT} SHARED
    ${HEADERS}
    ${SOURCES}
)

TARGET_LINK_LIBRARIES(${PROJECT}
    cocaine-core
)

ADD_DEFINITIONS(-std=c++11)
