cmake_minimum_required(VERSION 2.6)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

set(PROJECT cocaine-framework-cpp)
project(${PROJECT})

find_package(Boost 1.40 REQUIRED COMPONENTS system thread program_options)
find_package(Blackhole REQUIRED)
find_package(Cocaine 0.12 REQUIRED)

add_subdirectory(src)
add_subdirectory(example/echo)
add_subdirectory(tst)

install(
    DIRECTORY
        include/
    DESTINATION include
    COMPONENT development
    PATTERN "detail" EXCLUDE)
