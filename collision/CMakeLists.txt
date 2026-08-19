cmake_minimum_required(VERSION 3.16)
project(collision_test_env CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include_directories(include)

add_executable(test_correctness test/test_correctness.cpp)
add_executable(bench_k_sweep bench/bench_k_sweep.cpp)
add_executable(bench bench/bench.cpp)