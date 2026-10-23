project(test_simulation)

add_executable(simulation_smoke_test simulation_smoke_test.cpp)

target_link_libraries(simulation_smoke_test PRIVATE
    baselib
    simulation
)

add_test(NAME simulation_smoke_test COMMAND simulation_smoke_test)
