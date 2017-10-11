cmake_minimum_required(VERSION 2.6)
project(test)

SET(CMAKE_CXX_FLAGS "-D_LINUX -std=c++14 -O2 -g -Wall -Wno-unknown-pragmas")

include_directories(/media/denis/tmp/bb_ai/nanovg/nanovg/src/)

add_executable(test main.cpp mystrategy.cpp mystrategy.hpp /media/denis/tmp/bb_ai/nanovg/nanovg/src/nanovg.c Strategy.cpp Strategy.hpp)

install(TARGETS test RUNTIME DESTINATION bin)

target_link_libraries(test
    GL GLEW SDL2 SDL2_image boost_system boost_timer
)

