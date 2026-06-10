project(test_framework)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

add_executable(framework_smoke_test framework_smoke_test.cpp)

target_link_libraries(framework_smoke_test PRIVATE
    baselib
    logging
    config
    event
    scheduler
    runtime
)

add_test(NAME framework_smoke_test COMMAND framework_smoke_test)
