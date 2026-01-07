project(communicate_module)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${SUBLIB_OUT_PATH}/bin)

# 选项配置
option(COMMUNICATE_USE_ASIO "Use ASIO for network operations" ON)
option(COMMUNICATE_USE_DDS "Use DDS protocol support" ON)

# 源文件
file(GLOB COMMUNICATE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
list(APPEND COMMUNICATE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/base_adapter.cpp)

if(COMMUNICATE_USE_ASIO)
    list(APPEND COMMUNICATE_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/tcp_adapter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/udp_adapter.cpp
    )
endif()
if(COMMUNICATE_USE_DDS)
    list(APPEND COMMUNICATE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/dds_adapter.cpp)
endif()

# 构建库
add_library(${PROJECT_NAME} SHARED ${COMMUNICATE_SOURCES})

# 链接依赖库
if(COMMUNICATE_USE_ASIO)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${3RD_DIR}/asio-1.36.0/asio/include
    )

    # 添加 Windows Socket 库链接
    if(WIN32)
        target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 mswsock)
    endif()

    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_ASIO=1)
endif()
if(COMMUNICATE_USE_DDS)
    add_subdirectory(${3RD_DIR}/cyclonedds-0.10.5 ${CMAKE_CURRENT_BINARY_DIR}/cyclonedds)
    target_link_libraries(${PROJECT_NAME} PRIVATE ddsc)

    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_DDS=1)
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE baselib)

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
