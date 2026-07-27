cmake_minimum_required(VERSION 3.15)
project(nbody_sim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Debug模式加開錯誤檢查，Release模式加最佳化
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# 核心library：把所有共用邏輯打包成一個library，
# 之後 apps/ 跟 tests/ 都只要 link 這個 library，不用重複列檔案
add_library(nbody_core
    src/brute_force.cpp
)

target_include_directories(nbody_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# 兩體驗證demo
add_executable(two_body_demo apps/two_body_demo.cpp)
target_link_libraries(two_body_demo PRIVATE nbody_core)