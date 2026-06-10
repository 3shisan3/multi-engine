project(runtime)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${SUBLIB_OUT_PATH}/bin)

file(GLOB_RECURSE RUNTIME_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(${PROJECT_NAME} STATIC ${RUNTIME_SOURCES})

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        baselib
    PRIVATE
        logging
        config
        event
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/interface>
        $<BUILD_INTERFACE:${MODULES_DIR}/baselib/interface>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
