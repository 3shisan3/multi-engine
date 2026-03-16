project(plugin_module)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${SUBLIB_OUT_PATH}/bin)

# 源文件
file(GLOB_RECURSE PLUGIN_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# 构建库
add_library(${PROJECT_NAME} SHARED ${PLUGIN_SOURCES})

# 指定公开包含目录
target_include_directories(${PROJECT_NAME}
    PUBLIC
        # 对外暴露的接口目录
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/interface>
        $<INSTALL_INTERFACE:include>
    
    PRIVATE
        # 私有包含目录
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
