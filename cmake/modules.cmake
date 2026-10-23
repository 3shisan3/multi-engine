# ==============================================================================
# 框架模块
#
# 新增模块时：在 modules/ 下按 interface / include / src 布局创建目录，
# 然后在此处按开关 add_subdirectory 即可。
# ==============================================================================

set(MODULES_DIR ${CMAKE_SOURCE_DIR}/modules)

# 基础层，始终构建
add_subdirectory(${MODULES_DIR}/baselib)

# 通用能力层
if (ME_ENABLE_LOGGING)
    add_subdirectory(${MODULES_DIR}/logging)
endif()

if (ME_ENABLE_CONFIG)
    add_subdirectory(${MODULES_DIR}/config)
endif()

if (ME_ENABLE_EVENT)
    add_subdirectory(${MODULES_DIR}/event)
endif()

if (ME_ENABLE_METRICS)
    add_subdirectory(${MODULES_DIR}/metrics)
endif()

if (ME_ENABLE_SERIALIZATION)
    add_subdirectory(${MODULES_DIR}/serialization)
endif()

if (ME_ENABLE_RESOURCE)
    add_subdirectory(${MODULES_DIR}/resource)
endif()

# 运行时层
if (ME_ENABLE_SCHEDULER)
    add_subdirectory(${MODULES_DIR}/scheduler)
endif()

if (ME_ENABLE_RUNTIME)
    add_subdirectory(${MODULES_DIR}/runtime)
endif()

if (ME_ENABLE_PLUGIN)
    add_subdirectory(${MODULES_DIR}/plugin)
endif()

if (ME_ENABLE_COMMUNICATION)
    add_subdirectory(${MODULES_DIR}/communication)
endif()
