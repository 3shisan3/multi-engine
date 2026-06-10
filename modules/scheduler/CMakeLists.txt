project(scheduler)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${SUBLIB_OUT_PATH}/bin)

file(GLOB_RECURSE SCHEDULER_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(${PROJECT_NAME} STATIC ${SCHEDULER_SOURCES})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        baselib
        logging
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/interface>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
