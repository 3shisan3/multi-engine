# multi-engine

基于 C++17 与 CMake 的模块化多引擎框架模板，面向仿真系统、分布式游戏服务器、数字孪生、机器人集群、多节点控制系统以及插件化业务平台。

项目目标不是实现某个固定业务系统，而是沉淀一套可复用的工程底座：

- 统一应用启动与生命周期管理
- 统一模块加载、依赖管理和服务注册
- 支持多协议通信与内部事件总线
- 支持插件扩展
- 支持配置、日志、调度等通用工程能力

> 设计细节见 [docs/architecture_design.md](docs/architecture_design.md)。

---

## 目录结构

```text
multi-engine
├─ CMakeLists.txt          # 顶层构建，支持模块开关
├─ modules/                # 框架模块
│  ├─ baselib/             # 基础层：单例、线程池、模块生命周期接口
│  ├─ logging/             # 基于 spdlog 的日志封装
│  ├─ config/              # 基于 nlohmann_json 的 JSON 配置服务
│  ├─ event/               # 进程内事件总线（发布订阅）
│  ├─ metrics/             # 指标统计（计数器/仪表盘/耗时观测）
│  ├─ serialization/       # 消息编解码（小端序二进制协议）
│  ├─ resource/            # 资源管理（提供者注册、缓存、引用回收）
│  ├─ scheduler/           # 主循环调度器 / 事件调度器
│  ├─ runtime/             # 应用上下文、服务注册表、模块管理器、应用入口
│  ├─ communication/       # 多协议通信枢纽（TCP/UDP/DDS）、内部通信枢纽
│  └─ plugin/              # 插件接口、插件管理器、动态库加载器
├─ tests/                  # 自动化验证
│  ├─ framework/           # 框架链路冒烟测试
│  ├─ communication/       # 通信模块游戏服务器示例 + Python 冒烟测试
│  ├─ metrics/             # 指标统计模块冒烟测试
│  ├─ serialization/       # 消息编解码模块冒烟测试
│  ├─ resource/            # 资源管理模块冒烟测试
│  └─ plugin/              # 插件加载/卸载冒烟测试（含示例插件）
├─ thirdparty/             # 三方依赖（submodule）
│  ├─ asio/                # TCP/UDP 网络
│  ├─ cyclonedds/          # DDS 协议
│  ├─ nlohmann_json/       # JSON
│  └─ spdlog/              # 日志
└─ docs/                   # 设计文档
```

模块统一采用如下布局：

```text
modules/<module>
├─ CMakeLists.txt
├─ interface/    # 对外公开接口
├─ include/      # 模块内部头文件
└─ src/          # 实现文件
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
| `ME_ENABLE_LOGGING` | ON | 日志模块 |
| `ME_ENABLE_CONFIG` | ON | 配置模块 |
| `ME_ENABLE_EVENT` | ON | 事件模块 |
| `ME_ENABLE_METRICS` | ON | 指标统计模块 |
| `ME_ENABLE_SERIALIZATION` | ON | 消息编解码模块 |
| `ME_ENABLE_RESOURCE` | ON | 资源管理模块 |
| `ME_ENABLE_SCHEDULER` | ON | 调度模块 |
| `ME_ENABLE_RUNTIME` | ON | 运行时模块 |
| `ME_ENABLE_PLUGIN` | ON | 插件模块 |
| `ME_ENABLE_COMMUNICATION` | ON | 通信模块 |
| `COMMUNICATE_USE_ASIO` | ON | 使用 asio 实现 TCP/UDP |
| `COMMUNICATE_USE_DDS` | ON | 使用 DDS（依赖 CycloneDDS） |

所有构建产物统一输出到 `<build>/bin`，保证可执行文件与动态库同目录，避免运行时查找 DLL 失败。

---

## 测试

```bash
cd build
ctest --output-on-failure
```

当前测试：

| 测试 | 说明 |
| --- | --- |
| `framework_smoke_test` | 验证 logging/config/event/scheduler/runtime 链路 |
| `communication_smoke_test` | 启动游戏服务器示例，Python 客户端完成 TCP 登录/消息流程 |
| `metrics_smoke_test` | 验证计数器/仪表盘/耗时观测、序列化与多线程安全 |
| `serialization_smoke_test` | 验证基本类型/字符串/字节数组编解码、小端序与越界错误处理 |
| `resource_smoke_test` | 验证提供者注册、缓存命中、引用回收、统计与并发获取 |
| `plugin_smoke_test` | 加载示例插件 DLL，验证启动/命令/停止/卸载全链路 |

---

## 模块依赖关系

```text
baselib
  ↓
logging / config / event / metrics / serialization / resource
  ↓
runtime / scheduler / communication / plugin
```

- `baselib` 不依赖其他内部模块。
- `runtime` 负责编排生命周期，不承载具体业务模型。
- `communication` 不定义业务命令，网络消息应转换为领域命令后交给业务模块。
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
