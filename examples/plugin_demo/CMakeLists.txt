project(plugin_demo)

# 示例插件：动态库，导出 CreatePlugin / DestroyPlugin
add_library(demo_plugin SHARED demo_plugin.cpp)
target_include_directories(demo_plugin PRIVATE
    ${MODULES_DIR}/plugin/interface
)

# 宿主程序
add_executable(plugin_demo_host plugin_demo_host.cpp)
target_link_libraries(plugin_demo_host PRIVATE
    plugin_module
)
target_include_directories(plugin_demo_host PRIVATE
    ${MODULES_DIR}/plugin/interface
)
