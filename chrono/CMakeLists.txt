cmake_minimum_required(VERSION 3.10)

if(NOT
   DEFINED
   PROJECT_NAME
)
    project(Chrono)
endif()

set(CMAKE_CXX_STANDARD
    11
)
# chrono lib
add_library(
    chrono STATIC
    src/chrono.cpp src/hist.cpp
)

target_sources(
    chrono
    PRIVATE include_private/versions.h
            include/chrono.h
            include/hist.h
)

target_include_directories(
    chrono
    PRIVATE include_private
)

target_include_directories(
    chrono
    PUBLIC include
)
