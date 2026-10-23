project(test_resource)

add_executable(resource_smoke_test resource_smoke_test.cpp)

target_link_libraries(resource_smoke_test PRIVATE
    baselib
    resource
)

add_test(NAME resource_smoke_test COMMAND resource_smoke_test)
