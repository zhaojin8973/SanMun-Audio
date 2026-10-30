cmake_minimum_required(VERSION 3.22)
project(SanMuReverb VERSION 1.0.4 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)
option(SANMU_COPY_PLUGIN_AFTER_BUILD "构建后复制到系统插件目录" OFF)

# 添加 JUCE 9.0
# 系列统一仓库使用根目录锁定的 JUCE，旧单工程 checkout 仍可单独构建。
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../external/JUCE/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../external/JUCE"
        "${CMAKE_CURRENT_BINARY_DIR}/external/JUCE")
else()
    add_subdirectory(external/JUCE)
endif()

# 添加源码目录
add_subdirectory(source)
