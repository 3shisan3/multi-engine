# ==============================================================================
# 三方依赖
# ==============================================================================

set(3RD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)

add_subdirectory(${3RD_DIR}/nlohmann_json-3.12.0)
add_subdirectory(${3RD_DIR}/spdlog-1.17.0)
# add_subdirectory(${3RD_DIR}/zlib-1.3.1)
