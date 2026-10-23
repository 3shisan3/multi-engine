project(simulation)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${SUBLIB_OUT_PATH}/bin)

# 源文件
file(GLOB_RECURSE SIMULATION_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# 构建库（仅依赖 baselib）
add_library(${PROJECT_NAME} STATIC ${SIMULATION_SOURCES})

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
