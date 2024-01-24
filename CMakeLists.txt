cmake_minimum_required(VERSION 3.14)
project(capstone_host)

add_executable(host "main.cpp" "scene.cpp" "defs.hpp" "utils.hpp" "bvtree.cpp")
add_subdirectory(ext/IO)

set_property(TARGET host PROPERTY CXX_STANDARD 23)
set_property(TARGET host PROPERTY CXX_STANDARD_REQUIRED)
target_compile_definitions(host PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_definitions(host PRIVATE _CRT_SECURE_NO_DEPRECATE)

include(FetchContent)

FetchContent_Declare(cxxopts
    GIT_REPOSITORY  https://github.com/jarro2783/cxxopts
    GIT_TAG         origin/master)
FetchContent_MakeAvailable(cxxopts)

FetchContent_Declare(rapidobj
    GIT_REPOSITORY  https://github.com/guybrush77/rapidobj.git
    GIT_TAG         origin/master)
FetchContent_MakeAvailable(rapidobj)

target_link_libraries(host PRIVATE io)
target_link_libraries(host PRIVATE cxxopts)
target_link_libraries(host PRIVATE rapidobj::rapidobj)