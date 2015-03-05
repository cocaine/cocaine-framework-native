if(APPLE)
    set(CMAKE_MACOSX_RPATH ON)
endif()

set(Boost_USE_MULTITHREADED ON)

set(PROJECT cocaine-framework-cpp)

set(COCAINE_FRAMEWORK_INCLUDES_DIR
    ${CMAKE_SOURCE_DIR}/include/cocaine/framework
    ${CMAKE_SOURCE_DIR}/src)

set(SOURCES
    basic_session
    net
    decoder
    error
    log
    manager
    message
    scheduler
    resolver
    sender
    session
    service
    shared_state
    receiver
    worker.cpp
    worker/dispatch
    worker/options
    worker/sender
    worker/receiver
    worker/session

    util/future/error
)

project(${PROJECT})

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    SYSTEM ${CMAKE_SOURCE_DIR}/foreign/asio/asio/include
)

add_library(${PROJECT} SHARED ${SOURCES})

target_link_libraries(${PROJECT}
    ${Boost_LIBRARIES}
    msgpack
)

set_target_properties(${PROJECT} PROPERTIES
    VERSION 2
    COMPILE_FLAGS "-Wall -pedantic -Wextra -Winit-self -Wconversion -Wno-sign-conversion -Wold-style-cast -Woverloaded-virtual -Wctor-dtor-privacy -Wnon-virtual-dtor"
)

add_definitions(-std=c++0x)

install(
    TARGETS
        cocaine-framework-cpp
    LIBRARY DESTINATION lib)
