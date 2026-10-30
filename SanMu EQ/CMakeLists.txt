# ==============================================================================
# SanMuQ6 - 4 段参量均衡器插件
# 顶层 CMake 构建配置
# ==============================================================================
cmake_minimum_required(VERSION 3.25)
project(SanMuQ6 VERSION 0.1.5 LANGUAGES C CXX)

# ---- C++ 标准 ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- 添加 JUCE 框架 ----
# 系列统一仓库使用根目录锁定的 JUCE，旧单工程 checkout 仍可单独构建。
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../external/JUCE/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../external/JUCE"
        "${CMAKE_CURRENT_BINARY_DIR}/external/JUCE")
else()
    add_subdirectory(external/JUCE)
endif()

# ---- 添加插件源码 ----
add_subdirectory(source)
