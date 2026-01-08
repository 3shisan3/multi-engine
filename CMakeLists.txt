cmake_minimum_required(VERSION 3.10)

project(simulation_engine)

option(BUILD_TESTING "Build tests" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 指定编译输出的路径
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
set(SUBLIB_OUT_PATH ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/inner)

# 基础变量定义
set(MODULES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/modules)
set(3RD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)

# 三方依赖包含
add_subdirectory(${3RD_DIR}/nlohmann_json-3.12.0)
add_subdirectory(${3RD_DIR}/spdlog-1.17.0)
# add_subdirectory(${3RD_DIR}/zlib-1.3.1)

# 模块包含
add_subdirectory(${MODULES_DIR}/baselib)
add_subdirectory(${MODULES_DIR}/communication)

# 测试包含
if (BUILD_TESTING)
    add_subdirectory(tests)
endif()

