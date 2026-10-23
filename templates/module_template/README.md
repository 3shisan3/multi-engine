# 模块开发模板

新增框架模块时，以本目录为骨架派生，可保证目录结构与构建方式一致。

## 使用步骤

1. **复制目录**

   ```bash
   cp -r templates/module_template modules/<your_module>
   ```

2. **重命名占位符**

   将模板中的 `my_module` / `MyModule` / `MY_MODULE` 替换为你的模块标识：

   | 占位符 | 含义 | 示例 |
   | --- | --- | --- |
   | `my_module` | CMake 项目名 / 目录名 | `world` |
   | `MyModule` | C++ 命名空间 | `WorldModule` |
   | `MyModule.h` | 对外接口头文件名 | `World.h` |
   | `IMyService` | 对外接口类名 | `IWorldService` |
   | `MY_MODULE_INTERFACE_H` | 头文件保护宏 | `ME_WORLD_WORLD_H` |

3. **接入构建**

   在 `cmake/modules.cmake` 中按开关加入：

   ```cmake
   if (ME_ENABLE_WORLD)
       add_subdirectory(${MODULES_DIR}/world)
   endif()
   ```

   并在 `cmake/options.cmake` 增加开关：

   ```cmake
   option(ME_ENABLE_WORLD "Enable world module" ON)
   ```

4. **补充测试**

   在 `tests/` 下新增对应冒烟测试。

## 目录职责

| 目录 | 职责 |
| --- | --- |
| `interface/` | 对外公开接口，只有这里会被其他模块 include |
| `include/` | 模块内部实现头文件，不对外暴露 |
| `src/` | 实现文件 |

## 设计约束

- 基础层模块（如 `baselib`）不依赖其他内部模块。
- 通用能力层模块保持最小依赖，优先只依赖 `baselib`。
- 运行时层负责编排，不承载具体业务模型。
- 数据模型不直接绑定序列化细节。
