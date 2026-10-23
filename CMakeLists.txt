cmake_minimum_required(VERSION 3.10)

project(simulation_engine)

# 构建脚本分别收敛到 cmake/ 目录，便于维护与复用
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(options)
include(dependencies)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 指定编译输出的路径（统一到构建目录，保证 exe 与 dll 同目录，便于运行与查找）
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(SUBLIB_OUT_PATH ${CMAKE_BINARY_DIR})

# 框架模块
include(modules)

# 测试包含
if (BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
