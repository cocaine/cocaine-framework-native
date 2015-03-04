cmake_minimum_required(VERSION 2.6)

set(PROJECT cocaine-framework-cpp)
project(${PROJECT})

find_package(Boost 1.40 REQUIRED COMPONENTS system thread program_options)

add_subdirectory(src)
add_subdirectory(example/echo)
add_subdirectory(tst)

install(
    DIRECTORY
        include/
    DESTINATION include
    COMPONENT development
    PATTERN "detail" EXCLUDE)
