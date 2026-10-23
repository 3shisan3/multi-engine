project(metrics)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${SUBLIB_OUT_PATH}/bin)

# 源文件
file(GLOB_RECURSE METRICS_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# 构建库
add_library(${PROJECT_NAME} STATIC ${METRICS_SOURCES})

# 序列化依赖 nlohmann_json
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        baselib
    PRIVATE
        nlohmann_json
)

# 指定公开包含目录
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/interface>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
