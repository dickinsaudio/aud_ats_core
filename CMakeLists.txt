cmake_minimum_required(VERSION 3.10)

project(ATScomponent)

set(CMAKE_VERBOSE_MAKEFILE
    TRUE
)

set(COMPONENT_VERSION_TAG_PREFIX
    ats_core_v
)



# Load chrono first, as ats_core depends on it
add_subdirectory(chrono)
#add_dependencies(chrono chronoversion)
add_subdirectory(ats_core)
#add_dependencies(ats atsversion)

target_compile_options(
    ats
    PRIVATE -Wall -Wextra
)
