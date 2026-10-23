# ==============================================================================
# 模块构建脚本模板
#
# 使用方式：
#   1. 将整个 module_template 目录复制为 modules/<your_module>
#   2. 将下方 project(my_module) 与 MY_MODULE_SOURCES 中的 my_module
#      替换为你的模块名
#   3. 在 cmake/modules.cmake 中按需 add_subdirectory
# ==============================================================================

project(my_module)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${SUBLIB_OUT_PATH}/bin)

# 源文件
file(GLOB_RECURSE MY_MODULE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# 构建库
add_library(${PROJECT_NAME} STATIC ${MY_MODULE_SOURCES})

# 依赖：基础层用 PUBLIC，内部实现用 PRIVATE
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        baselib
)

# 指定公开包含目录
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/interface>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
