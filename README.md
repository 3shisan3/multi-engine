# multi-engine

基于 C++17 与 CMake 的模块化多引擎框架模板，面向仿真系统、分布式游戏服务器、数字孪生、机器人集群、多节点控制系统以及插件化业务平台。

项目定位是**模板仓库**：只保留核心模块，聚焦通用底座能力，保证流程可串联跑通，不堆砌细分功能。

> 设计细节见 [docs/architecture_design.md](docs/architecture_design.md)。

---

## 核心模块

| 模块 | 职责 |
| --- | --- |
| `baselib` | 单例、线程池、模块生命周期接口 |
| `logging` | 日志封装（spdlog）+ 指标统计 |
| `config` | JSON 配置加载与点路径访问 |
| `event` | 进程内事件总线（发布订阅） |
| `scheduler` | 主循环调度器 / 事件调度器 |
| `runtime` | 应用上下文、服务注册表、模块管理器、应用入口 |
| `communication` | 多协议通信枢纽（TCP/UDP/DDS）+ 消息编解码 |
| `plugin` | 插件接口、插件管理器、动态库加载 |

模块统一布局：

```text
modules/<module>
├─ CMakeLists.txt
├─ interface/    # 对外公开接口
├─ include/      # 模块内部头文件
└─ src/          # 实现文件
```

---

## 核心流程

```text
config → logging → event → scheduler → runtime → communication → plugin
```

应用通过 `runtime` 编排以上能力，`examples/minimal_app` 给出最小串联示例。

---

## 目录结构

```text
multi-engine
├─ CMakeLists.txt          # 顶层入口（仅编排）
├─ cmake/                  # 构建脚本
│  ├─ options.cmake        #   模块开关
│  ├─ dependencies.cmake   #   三方依赖
│  └─ modules.cmake        #   模块包含
├─ configs/                # 默认配置
│  ├─ default_engine.json
│  ├─ default_logging.json
│  └─ default_communication.json
├─ examples/               # 示例工程
│  ├─ minimal_app/         #   最小应用（串联核心模块）
│  └─ plugin_demo/         #   插件开发示例
├─ templates/              # 模块模板
│  └─ module_template/
├─ modules/                # 框架模块（8 个）
│  ├─ baselib/
│  ├─ logging/
│  ├─ config/
│  ├─ event/
│  ├─ scheduler/
│  ├─ runtime/
│  ├─ communication/
│  └─ plugin/
├─ tests/                  # 自动化验证
│  ├─ framework/           #   框架链路冒烟测试
│  ├─ communication/       #   通信示例 + 消息编解码测试
│  ├─ logging/             #   日志与指标统计测试
│  └─ plugin/              #   插件加载/卸载测试
├─ thirdparty/             # 三方依赖（submodule）
│  ├─ asio/                # TCP/UDP 网络
│  ├─ cyclonedds/          # DDS 协议
│  ├─ nlohmann_json/       # JSON
│  └─ spdlog/              # 日志
└─ docs/                   # 设计文档
```

---

## 环境依赖

- CMake >= 3.10
- 支持 C++17 的编译器（GCC / Clang / MSVC / MinGW-w64）
- Python 3（仅通信模块冒烟测试需要，可选）

三方依赖通过 git submodule 管理，首次使用需初始化：

```bash
git submodule update --init --recursive
```

---

## 构建

### Windows（MinGW-w64）

```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug \
      -DCOMMUNICATE_USE_DDS=OFF
cmake --build build -j8
```

### Linux / macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug \
      -DCOMMUNICATE_USE_DDS=OFF
cmake --build build -j$(nproc)
```

> 说明：CycloneDDS 依赖较重且跨平台配置复杂，默认建议先关闭 DDS（`-DCOMMUNICATE_USE_DDS=OFF`）完成常规构建与测试。
> 通信模块的 TCP/UDP 基于 header-only 的 asio，无需额外安装。

### 常用构建选项

| 选项 | 默认 | 说明 |
| --- | --- | --- |
| `BUILD_TESTING` | ON | 是否构建测试 |
| `ME_ENABLE_LOGGING` | ON | 日志模块（含指标统计） |
| `ME_ENABLE_CONFIG` | ON | 配置模块 |
| `ME_ENABLE_EVENT` | ON | 事件模块 |
| `ME_ENABLE_SCHEDULER` | ON | 调度模块 |
| `ME_ENABLE_RUNTIME` | ON | 运行时模块 |
| `ME_ENABLE_PLUGIN` | ON | 插件模块 |
| `ME_ENABLE_COMMUNICATION` | ON | 通信模块（含消息编解码） |
| `COMMUNICATE_USE_ASIO` | ON | 使用 asio 实现 TCP/UDP |
| `COMMUNICATE_USE_DDS` | ON | 使用 DDS（依赖 CycloneDDS） |
| `ME_BUILD_EXAMPLES` | OFF | 是否构建示例工程 |

所有构建产物统一输出到 `<build>/bin`，保证可执行文件与动态库同目录。

顶层 `CMakeLists.txt` 只做编排，具体构建逻辑拆分在 `cmake/` 目录：

| 脚本 | 职责 |
| --- | --- |
| `cmake/options.cmake` | 模块开关（`ME_ENABLE_*`） |
| `cmake/dependencies.cmake` | 三方依赖引入 |
| `cmake/modules.cmake` | 按开关包含框架模块 |

---

## 测试

```bash
cd build
ctest --output-on-failure
```

| 测试 | 说明 |
| --- | --- |
| `framework_smoke_test` | 验证 logging/config/event/scheduler/runtime 链路 |
| `logging_smoke_test` | 验证日志初始化与指标统计（计数器/仪表盘/观测/序列化/线程安全） |
| `message_codec_test` | 验证消息编解码（基本类型/字符串/字节、小端序、越界错误） |
| `communication_smoke_test` | 启动游戏服务器示例，Python 客户端完成 TCP 登录/消息流程 |
| `plugin_smoke_test` | 加载示例插件 DLL，验证启动/命令/停止/卸载全链路 |

---

## 模块依赖关系

```text
baselib
  ↓
logging / config / event
  ↓
runtime / scheduler / communication / plugin
```

- `baselib` 不依赖其他内部模块。
- `logging` 内聚日志与指标统计，不引入额外业务依赖。
- `runtime` 负责编排生命周期，不承载具体业务模型。
- `communication` 负责传输与编解码，不定义业务命令。
- 示例代码不进入框架核心，自动化验证放在 `tests`。

---

## 典型使用方式

应用通过 `runtime` 模块组合能力：

```cpp
#include "Application.h"
#include "Logger.h"

using namespace RuntimeModule;

int main()
{
    LogModule::Logger::InitializeDefault("logs");

    Application app;
    app.SetConfigureCallback([](EngineContext& ctx) {
        ctx.Services().RegisterService("my_service", nullptr);
        return true;
    });
    app.SetMainLoopCallback([](EngineContext& ctx) {
        // 主循环逻辑
        return 0;
    });

    return app.Run();
}
```

---

## 许可

[MIT](LICENSE)
