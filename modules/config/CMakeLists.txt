project(config)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${SUBLIB_OUT_PATH}/bin)

file(GLOB_RECURSE CONFIG_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(${PROJECT_NAME} STATIC ${CONFIG_SOURCES})

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        nlohmann_json::nlohmann_json
    PRIVATE
        baselib
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/interface>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
