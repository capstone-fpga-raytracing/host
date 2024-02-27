cmake_minimum_required(VERSION 3.14)
project(rthost)

add_executable(rthost "main.cpp" "scene.cpp" "defs.hpp" "utils.hpp")
add_subdirectory(ext/IO)

include(FetchContent)

FetchContent_Declare(cxxopts
    GIT_REPOSITORY  https://github.com/jarro2783/cxxopts
    GIT_TAG         origin/master)
FetchContent_MakeAvailable(cxxopts)

FetchContent_Declare(rapidobj
    GIT_REPOSITORY  https://github.com/guybrush77/rapidobj.git
    GIT_TAG         origin/master)
FetchContent_MakeAvailable(rapidobj)

target_link_libraries(rthost PRIVATE io)
target_link_libraries(rthost PRIVATE cxxopts)
target_link_libraries(rthost PRIVATE rapidobj::rapidobj)

set_property(TARGET rthost PROPERTY CXX_STANDARD 23)
set_property(TARGET rthost PROPERTY CXX_STANDARD_REQUIRED)
target_compile_definitions(rthost PRIVATE _CRT_SECURE_NO_WARNINGS)

if (NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, default to Release")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()
