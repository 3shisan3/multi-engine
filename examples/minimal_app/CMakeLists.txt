project(minimal_app)

add_executable(minimal_app main.cpp)

target_link_libraries(minimal_app PRIVATE
    baselib
    logging
    config
    event
    metrics
    runtime
)

target_include_directories(minimal_app PRIVATE
    ${MODULES_DIR}/baselib/interface
)
