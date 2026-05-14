cmake_minimum_required(VERSION 3.16)
project(a)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# exe file
add_executable( a
    src/main.cpp
    src/simulation.cpp
)

# /include path (body.hpp, simulation.hpp, glm)
target_include_directories( a PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)