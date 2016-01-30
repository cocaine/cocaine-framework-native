if(APPLE)
    set(CMAKE_MACOSX_RPATH ON)
endif()

include(AddCXXCompilerFlag)

find_package(Boost 1.40 REQUIRED COMPONENTS system thread program_options)
find_package(Cocaine 0.12 REQUIRED)

if(WITH_TRACE)
    find_package(Blackhole REQUIRED)
endif()

set(Boost_USE_MULTITHREADED ON)

set(PROJECT cocaine-framework-native)

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
    trace.cpp
    trace_logger.cpp
    worker.cpp
    worker/dispatch
    worker/error
    worker/options
    worker/sender
    worker/session
    worker/receiver

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
    cocaine-core # Temporary link until required.
    msgpack
    blackhole
)

# Try and enable C++11.
add_cxx_compiler_flag(-std=c++11)
IF(NOT HAVE_CXX_FLAG_STD_CXX11)
  add_cxx_compiler_flag(-std=c++0x)
ENDIF()

# Turn compiler warnings.
add_cxx_compiler_flag(-Wall)
add_cxx_compiler_flag(-Wextra)
add_cxx_compiler_flag(-Werror RELEASE)
add_cxx_compiler_flag(-Winit-self)
add_cxx_compiler_flag(-Wconversion)
add_cxx_compiler_flag(-Wno-sign-conversion)
add_cxx_compiler_flag(-Wold-style-cast)
add_cxx_compiler_flag(-Woverloaded-virtual)
add_cxx_compiler_flag(-Wctor-dtor-privacy)
add_cxx_compiler_flag(-Wnon-virtual-dtor)
add_cxx_compiler_flag(-Wunreachable-code)
add_cxx_compiler_flag(-pedantic)
add_cxx_compiler_flag(-pedantic-errors)

set_target_properties(${PROJECT} PROPERTIES
    VERSION 1
)

# Install section.
install(
    TARGETS
        cocaine-framework-native
    LIBRARY DESTINATION lib)

install(
    DIRECTORY
        ${CMAKE_SOURCE_DIR}/include/
    DESTINATION include
    COMPONENT development
    PATTERN "detail" EXCLUDE)
