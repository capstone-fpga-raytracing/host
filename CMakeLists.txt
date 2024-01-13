cmake_minimum_required(VERSION 3.12)
project(capstone_host)

add_executable(host main.cpp "scene.cpp" "defs.hpp" "bvtree.cpp")
#target_include_directories(host PRIVATE eigen-3.4.0)
target_include_directories(host PRIVATE cxxopts/include)

set_property(TARGET host PROPERTY CXX_STANDARD 23)
set_property(TARGET host PROPERTY CXX_STANDARD_REQUIRED)

include(FetchContent)

FetchContent_Declare(rapidobj
    GIT_REPOSITORY  https://github.com/guybrush77/rapidobj.git
    GIT_TAG         origin/master)

FetchContent_MakeAvailable(rapidobj)

target_link_libraries(host PRIVATE rapidobj::rapidobj)

add_custom_command(
	TARGET host
	POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_CURRENT_SOURCE_DIR}/tests
		${CMAKE_CURRENT_BINARY_DIR}/tests
)