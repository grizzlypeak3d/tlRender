# A program built against the tlRender and feather-tk wheels, the way a
# project with its own bindings would: see check.py.
cmake_minimum_required(VERSION 3.31)
project(wheel-consumer LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

find_package(tlRender REQUIRED)

add_executable(wheel-consumer main.cpp)
target_link_libraries(wheel-consumer tlRender::tlTimeline)
