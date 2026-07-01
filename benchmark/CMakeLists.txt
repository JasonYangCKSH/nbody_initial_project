# 1. Project Name
cmake_minimum_required(VERSION 3.16)
project(benchmark LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1.5 Build type 預設 Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2") #---------------> good compile

# 2. add subdirectory
add_subdirectory(extern/TreeNSearch)

# 2.5 TreeNSearch 需要 AVX2
target_compile_options(TreeNSearch PRIVATE -mavx2) #----------> turn on avx2 mode

# 3. add benchmark(executable file)
add_executable(tests tests/main.cpp include/point_set.cpp include/simulation.cpp)
set_target_properties(tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/tests
)
# 3.5 OpenMP-----------------
find_package(OpenMP REQUIRED)
# ---------------------------

# 4. target include directories
target_include_directories(tests PRIVATE include)

# 5. target link libraries---
target_link_libraries(tests PRIVATE TreeNSearch OpenMP::OpenMP_CXX)
# ---------------------------
