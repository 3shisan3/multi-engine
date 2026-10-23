project(test_metrics)

add_executable(metrics_smoke_test metrics_smoke_test.cpp)

target_link_libraries(metrics_smoke_test PRIVATE
    baselib
    metrics
)

add_test(NAME metrics_smoke_test COMMAND metrics_smoke_test)
