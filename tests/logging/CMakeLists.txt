project(test_logging)

add_executable(logging_smoke_test logging_smoke_test.cpp)

target_link_libraries(logging_smoke_test PRIVATE
    baselib
    logging
)

add_test(NAME logging_smoke_test COMMAND logging_smoke_test)
