cmake_minimum_required(VERSION 3.14)
project(Collision CXX)

# 設定 C++ 標準為 C++17 (確保支援基礎語法與 glm)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(bench bench/bench.cpp include/simulation.cpp)

add_executable(test_correctness test/test_correctness.cpp include/simulation.cpp)
