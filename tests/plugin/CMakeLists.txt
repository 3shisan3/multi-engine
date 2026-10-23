project(test_plugin)

# 示例插件：动态库，导出 CreatePlugin / DestroyPlugin
add_library(sample_plugin SHARED sample_plugin.cpp)
target_include_directories(sample_plugin PRIVATE
    ${MODULES_DIR}/plugin/interface
)

# 宿主测试程序
add_executable(plugin_smoke_test plugin_smoke_test.cpp)
target_link_libraries(plugin_smoke_test PRIVATE
    plugin_module
)
target_include_directories(plugin_smoke_test PRIVATE
    ${MODULES_DIR}/plugin/interface
)

add_test(NAME plugin_smoke_test
    COMMAND plugin_smoke_test $<TARGET_FILE:sample_plugin>
)
