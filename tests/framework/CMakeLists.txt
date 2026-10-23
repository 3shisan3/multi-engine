project(test_framework)

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
