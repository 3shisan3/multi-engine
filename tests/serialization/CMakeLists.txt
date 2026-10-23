project(test_serialization)

add_executable(serialization_smoke_test serialization_smoke_test.cpp)

target_link_libraries(serialization_smoke_test PRIVATE
    baselib
    serialization
)

add_test(NAME serialization_smoke_test COMMAND serialization_smoke_test)
