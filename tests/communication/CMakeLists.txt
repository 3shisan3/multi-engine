project(test_communication)

set(TEST_PROJECT_NAME game_test CACHE INTERNAL "Name of the test project")
option(TEST_COMMUNICATION_USE_PYTHON "Use Python for communication tests" ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/${TEST_PROJECT_NAME})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/${TEST_PROJECT_NAME}/src)

file(GLOB TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_PROJECT_NAME}/src/*.cpp)

add_executable(${TEST_PROJECT_NAME} ${TEST_SOURCES})

target_link_libraries(${TEST_PROJECT_NAME} PRIVATE
    baselib
    communicate_module
    nlohmann_json
)

if(TEST_COMMUNICATION_USE_PYTHON)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME communication_smoke_test
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/communication_smoke_test.py
                --server $<TARGET_FILE:${TEST_PROJECT_NAME}>
                --config ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_PROJECT_NAME}/config/server_config.json
                --client ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_PROJECT_NAME}/test_client.py
        )
    endif()
else()
    # 创建客户端测试可执行文件
    add_executable(client_simulator
        ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_PROJECT_NAME}/client_simulator.cpp
    )

    target_link_libraries(client_simulator PRIVATE
        baselib
        communicate_module
        nlohmann_json
    )
    target_include_directories(client_simulator PRIVATE
        ${3RD_DIR}/asio-1.36.0/asio/include
    )
endif()