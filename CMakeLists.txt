cmake_minimum_required(VERSION 3.10)
project(BarnesHutNBody VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------
# 1. 智慧型尋找 GLM (相容 Windows 與 Linux/WSL)
# ----------------------------------------------------------------

# 優先尋找系統中的 GLM (Linux 的 apt install 會安裝在此)
find_package(glm QUIET)

if(glm_FOUND)
    message(STATUS "Found GLM via CMake package: ${GLM_INCLUDE_DIRS}")
else()
    # 如果是 Windows 或找不到系統套件，嘗試手動路徑
    # 這裡的 PATHS 可以列出你常用的 Windows 路徑，CMake 會自動跳過不正確的
    find_path(GLM_INCLUDE_DIR 
        NAMES glm/glm.hpp
        PATHS 
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "C:/libs/glm"
            "D:/vclib/glm"
    )
    
    if(GLM_INCLUDE_DIR)
        message(STATUS "Found GLM manually: ${GLM_INCLUDE_DIR}")
    else()
        message(FATAL_ERROR "GLM not found! Please install it (sudo apt install libglm-dev) or set GLM_INCLUDE_DIR.")
    endif()
endif()

# ----------------------------------------------------------------
# 2. 設定執行檔
# ----------------------------------------------------------------

# 把 .h 也列進去是為了讓 IDE (VS Code) 的標頭檔跳轉更順手
add_executable(a 
    src/main.cpp 
    include/body.h 
    include/octree.h 
    include/simulation.h
)

# 包含你的 src & include 目錄
target_include_directories(a PRIVATE src include)

# 根據尋找結果連結 GLM
if(TARGET glm::glm)
    # Linux/WSL 常用方式
    target_link_libraries(a PRIVATE glm::glm)
else()
    # Windows/手動路徑方式
    target_include_directories(a PRIVATE ${GLM_INCLUDE_DIR})
endif()

# ----------------------------------------------------------------
# 3. 針對編譯器設定優化參數 (MSVC vs GCC/Clang)
# ----------------------------------------------------------------

if(MSVC)
    target_compile_options(a PRIVATE /W4 /O2)
else()

    target_compile_options(a PRIVATE -Wall -O3 -ffast-math)
endif()