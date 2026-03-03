cmake_minimum_required(VERSION 3.10)
project(BarnesHutNBody VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------
# 1. 尋找 GLM
# ----------------------------------------------------------------
find_package(glm QUIET)

if(NOT glm_FOUND)
    find_path(GLM_INCLUDE_DIR 
        NAMES glm/glm.hpp
        PATHS 
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "C:/libs/glm"
            "D:/vclib/glm"
            "/usr/include"
            "/usr/local/include"
    )
    if(NOT GLM_INCLUDE_DIR)
        message(FATAL_ERROR "GLM not found! (sudo apt install libglm-dev)")
    endif()
endif()

# ----------------------------------------------------------------
# 2. 尋找 SFML (Linux 下確保已經 sudo apt install libsfml-dev)
# ----------------------------------------------------------------
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

# ----------------------------------------------------------------
# 3. 設定執行檔 (注意：只定義一次 add_executable)
# ----------------------------------------------------------------
add_executable(a 
    src/main.cpp 
    include/body.h 
    include/octree.h 
    include/simulation.h
)

# ----------------------------------------------------------------
# 4. 包含路徑與連結庫
# ----------------------------------------------------------------
target_include_directories(a PRIVATE src include)

# 連結 GLM
if(TARGET glm::glm)
    target_link_libraries(a PRIVATE glm::glm)
else()
    target_include_directories(a PRIVATE ${GLM_INCLUDE_DIR})
endif()

# 連結 SFML
target_link_libraries(a PRIVATE sfml-graphics sfml-window sfml-system)

# ----------------------------------------------------------------
# 5. 編譯優化參數 (針對效能)
# ----------------------------------------------------------------
if(MSVC)
    target_compile_options(a PRIVATE /W4 /O2 /Oi /Ot)
else()
    # 加入 -march=native 可以讓效能提升 (針對目前這台電腦優化)
    target_compile_options(a PRIVATE -Wall -O3 -ffast-math -march=native)
endif()