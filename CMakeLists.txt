CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(COCAINE-FRAMEWORK-NATIVE)

SET(COCAINE_VERSION_MAJOR   0)
SET(COCAINE_VERSION_MINOR   10)
SET(COCAINE_VERSION_RELEASE 3)

SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

FIND_PACKAGE(Boost 1.40.0 REQUIRED
    COMPONENTS
        program_options)

INCLUDE(cmake/locate_library.cmake)

LOCATE_LIBRARY(LIBEV "ev++.h" "ev" "libev")
LOCATE_LIBRARY(LIBMSGPACK "msgpack.hpp" "msgpack")
LOCATE_LIBRARY(LIBCOCAINE "cocaine/context.hpp" "cocaine-core")

INCLUDE_DIRECTORIES(
    ${Boost_INCLUDE_DIRS}
    ${LIBEV_INCLUDE_DIRS}
    ${LIBMSGPACK_INCLUDE_DIRS}
    ${LIBCOCAINE_INCLUDE_DIRS})

INCLUDE_DIRECTORIES(BEFORE
    ${PROJECT_SOURCE_DIR}/include)

LINK_DIRECTORIES(
    ${Boost_LIBRARY_DIRS}
    ${LIBEV_LIBRARY_DIRS}
    ${LIBMSGPACK_LIBRARY_DIRS}
    ${LIBCOCAINE_LIBRARY_DIRS})

ADD_LIBRARY(cocaine-framework SHARED
    src/service
    src/dispatch
    src/future
    src/handlers/functional
    src/handlers/http)

TARGET_LINK_LIBRARIES(cocaine-framework
    boost_program_options-mt
    ev
    msgpack)

SET_TARGET_PROPERTIES(cocaine-framework PROPERTIES
    VERSION 1
    COMPILE_FLAGS "-std=c++0x -W -Wall -Werror -pedantic")

INSTALL(
    TARGETS
        cocaine-framework
    RUNTIME DESTINATION bin COMPONENT runtime
    LIBRARY DESTINATION lib COMPONENT runtime
    ARCHIVE DESTINATION lib COMPONENT developement)

INSTALL(
    DIRECTORY
        include/
    DESTINATION include
    COMPONENT development)
