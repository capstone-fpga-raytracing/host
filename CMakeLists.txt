cmake_minimum_required(VERSION 3.12)
project(capstone_host)

add_executable(host main.cpp "model_io.cpp" "defs.hpp")
#target_include_directories(host PRIVATE eigen-3.4.0)

set_property(TARGET host PROPERTY CXX_STANDARD 20)
set_property(TARGET host PROPERTY CXX_STANDARD_REQUIRED)
