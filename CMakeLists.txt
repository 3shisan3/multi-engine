cmake_minimum_required(VERSION 3.10)

project(simulation_engine)

option(BUILD_TESTING "Build tests" ON)
option(ME_ENABLE_LOGGING "Enable logging module" ON)
option(ME_ENABLE_CONFIG "Enable config module" ON)
option(ME_ENABLE_EVENT "Enable event module" ON)
option(ME_ENABLE_SCHEDULER "Enable scheduler module" ON)
option(ME_ENABLE_RUNTIME "Enable runtime module" ON)
option(ME_ENABLE_PLUGIN "Enable plugin module" ON)
option(ME_ENABLE_COMMUNICATION "Enable communication module" ON)

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

if (ME_ENABLE_LOGGING)
    add_subdirectory(${MODULES_DIR}/logging)
endif()

if (ME_ENABLE_CONFIG)
    add_subdirectory(${MODULES_DIR}/config)
endif()

if (ME_ENABLE_EVENT)
    add_subdirectory(${MODULES_DIR}/event)
endif()

if (ME_ENABLE_SCHEDULER)
    add_subdirectory(${MODULES_DIR}/scheduler)
endif()

if (ME_ENABLE_RUNTIME)
    add_subdirectory(${MODULES_DIR}/runtime)
endif()

if (ME_ENABLE_PLUGIN)
    add_subdirectory(${MODULES_DIR}/plugin)
endif()

if (ME_ENABLE_COMMUNICATION)
    add_subdirectory(${MODULES_DIR}/communication)
endif()

# 测试包含
if (BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

