cmake_minimum_required(VERSION 3.16)
project(sph_project VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenMP REQUIRED)
include_directories(/usr/include/eigen3)  # 若系統路徑不同，請改為 Eigen 安裝路徑

add_executable(main src/main.cpp src/Particle.cpp src/UniformGrid.cpp src/algorithm.cpp)
target_link_libraries(main PUBLIC OpenMP::OpenMP_CXX)