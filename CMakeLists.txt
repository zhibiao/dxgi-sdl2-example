cmake_minimum_required(VERSION 3.10)

project(demo VERSION 1.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(
    "vendor\\SDL2-2.0.16\\include"
)

link_directories(
    "vendor\\SDL2-2.0.16\\lib"
)

link_libraries(
    d3d11.lib
    d3dcompiler.lib
    SDL2.lib 
    SDL2main.lib
)

add_executable(demo src/main.cpp)
