project(baselib)

# 源文件
file(GLOB_RECURSE BASE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# 构建库
add_library(${PROJECT_NAME} STATIC ${BASE_SOURCES})

# 指定公开包含目录
target_include_directories(${PROJECT_NAME}
    PUBLIC
        # 暴露的接口目录
        ${CMAKE_CURRENT_SOURCE_DIR}/interface
    
    PRIVATE
        # 内部头文件
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)