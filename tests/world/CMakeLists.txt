project(test_world)

add_executable(world_smoke_test world_smoke_test.cpp)

target_link_libraries(world_smoke_test PRIVATE
    baselib
    world
)

add_test(NAME world_smoke_test COMMAND world_smoke_test)
