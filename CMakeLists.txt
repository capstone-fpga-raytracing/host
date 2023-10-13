cmake_minimum_required(VERSION 3.12)
project(capstone_host)

add_executable(host main.cpp "model.cpp" "defs.hpp" "bvtree.cpp")
#target_include_directories(host PRIVATE eigen-3.4.0)
target_include_directories(host PRIVATE cxxopts/include)

set_property(TARGET host PROPERTY CXX_STANDARD 23)
set_property(TARGET host PROPERTY CXX_STANDARD_REQUIRED)
