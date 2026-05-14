cmake_minimum_required(VERSION 3.16)
project(baseline)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# exe file
add_executable( baseline
    src/main.cpp
    src/simulation.cpp
)

# /include path (body.hpp, simulation.hpp, glm)
target_include_directories( baseline PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)