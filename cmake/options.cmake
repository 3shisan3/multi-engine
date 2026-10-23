# ==============================================================================
# 模块构建开关
# ==============================================================================

option(BUILD_TESTING "Build tests" ON)

# 示例工程（默认关闭，需要时显式开启）
option(ME_BUILD_EXAMPLES "Build examples" OFF)

option(ME_ENABLE_LOGGING "Enable logging module" ON)
option(ME_ENABLE_CONFIG "Enable config module" ON)
option(ME_ENABLE_EVENT "Enable event module" ON)
option(ME_ENABLE_METRICS "Enable metrics module" ON)
option(ME_ENABLE_SERIALIZATION "Enable serialization module" ON)
option(ME_ENABLE_RESOURCE "Enable resource module" ON)
option(ME_ENABLE_SIMULATION "Enable simulation module" ON)
option(ME_ENABLE_SCHEDULER "Enable scheduler module" ON)
option(ME_ENABLE_RUNTIME "Enable runtime module" ON)
option(ME_ENABLE_PLUGIN "Enable plugin module" ON)
option(ME_ENABLE_COMMUNICATION "Enable communication module" ON)
