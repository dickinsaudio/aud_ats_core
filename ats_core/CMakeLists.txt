cmake_minimum_required(VERSION 3.10)

if(NOT
   DEFINED
   PROJECT_NAME
)
    project(ATScore)
    add_subdirectory(../chrono chrono)
else()
    if(NOT
       TARGET
       chrono
    )
        message(WARNING "chrono must be loaded before ATS")
    endif()
endif()
set(CMAKE_CXX_STANDARD
    11
)
add_definitions(-D_UNICODE)
add_definitions(-DUNICODE)
# Single library for now
add_library(
    ats STATIC
    src/ats.cpp
    src/ats_generic.cpp
    src/versions.c
)

target_sources(
    ats
    PRIVATE include_private/ats_t.h include_private/versions.h
)

target_include_directories(
    ats
    PUBLIC include
)

target_include_directories(
    ats
    PRIVATE include_private
)

target_link_libraries(
    ats
    PUBLIC chrono
)

