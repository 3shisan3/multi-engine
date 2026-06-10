# Multi Engine Framework 项目架构说明

## 1. 项目简介

`multi-engine` 是一个基于 C++17 和 CMake 的模块化多引擎框架模板，面向仿真系统、分布式游戏服务器、数字孪生、机器人集群、多节点控制系统以及插件化业务平台。

项目目标不是实现某一个固定业务系统，而是沉淀一套可复用的工程底座：

- 统一应用启动与生命周期管理
- 统一模块加载、依赖管理和服务注册
- 支持多协议通信与内部事件总线
- 支持插件扩展
- 支持配置、日志、调度等通用工程能力
- 支持进一步扩展为完整仿真引擎、游戏服务器或分布式控制系统

当前项目已经具备框架雏形，并开始从实例项目 `simulation_engine` 中抽象可复用能力，逐步形成“通用底座 + 可选领域模块 + 示例项目 + 工程模板”的项目结构。

## 2. 设计目标

`multi-engine` 希望解决的是中大型 C++ 仿真/分布式项目中的基础工程问题。

在实际业务中，仿真系统或多节点平台通常会同时需要：

- 通信层：TCP、UDP、DDS 等多协议通信
- 运行时：应用启动、停止、模块生命周期管理
- 事件层：模块间解耦通信
- 插件层：运行时扩展和动态加载
- 数据层：配置、资源、场景、模型数据
- 调度层：主循环、定时任务、固定时间步
- 观测层：日志、统计、性能埋点、回放
- 领域层：世界模型、实体系统、环境模型、想定编辑等

如果每个项目都重复实现这些能力，会导致工程结构分散、模块边界不清晰、通信与业务耦合、测试和扩展成本较高。

因此本项目的设计目标是：

1. 提供一套清晰、稳定、可扩展的 C++ 模块化工程模板。
2. 将通用工程能力沉淀为框架模块。
3. 将仿真、游戏、数字孪生等业务能力沉淀为可选领域模块。
4. 通过示例项目展示典型用法。
5. 通过模板目录支持快速派生新项目、新模块和新插件。

## 3. 项目定位

`multi-engine` 推荐定位为：

> 一个面向仿真与分布式应用的 C++17 多引擎框架模板。

它可以有两种使用方式：

1. **作为框架底座**：被外部业务工程引用，提供通信、插件、运行时、日志、配置、事件等基础能力。
2. **作为模板工程**：直接派生出新的仿真系统、游戏服务器、控制系统或工具应用。

项目不应绑定单一业务场景，而应尽量保持通用模板性。

## 4. 当前项目状态

当前仓库已经包含以下模块：

```text
modules
├─ baselib
├─ communication
├─ config
├─ event
├─ logging
├─ plugin
├─ runtime
└─ scheduler
```

### 已有基础能力

| 模块 | 当前能力 |
| --- | --- |
| `baselib` | 单例模板、通用模块生命周期接口 |
| `logging` | 基于 spdlog 的日志封装、日志分类、日志宏、统计信息 |
| `config` | 基于 nlohmann_json 的 JSON 配置加载和点路径访问 |
| `event` | 轻量事件总线、发布订阅、事件统计 |
| `scheduler` | 主循环调度器、定时/循环/条件事件调度器 |
| `runtime` | 应用上下文、服务注册表、模块管理器、应用启动流程 |
| `communication` | 多协议通信 Hub、TCP/UDP/DDS 适配器、客户端管理、主题发布订阅 |
| `plugin` | 插件接口、插件基类、插件管理器、动态库加载 |

当前也已经添加了基础 smoke test，用于验证 `logging/config/event/scheduler/runtime` 的基本链路。

## 5. 与实例项目的关系

参考实例项目：

```text
D:\CodeSpace\output_of_commuter\sources\linger\simulation_engine
```

该实例项目展示了一个更完整的仿真引擎雏形，包含：

```text
modules
├─ baselib
├─ communication
├─ core
├─ data
├─ editor
├─ entity
├─ environment
├─ logging_tracking
└─ world
```

实例项目体现的业务能力包括：

- 仿真生命周期控制
- 主循环调度
- 实体系统
- 世界模型
- 环境模型
- 场景数据
- 想定编辑
- 日志追踪
- 性能埋点
- 状态快照
- COP 态势生成

这些能力不应被原样复制到 `multi-engine` 中，而应按职责拆分：

```text
通用能力 → framework modules
仿真能力 → domain modules
示例代码 → examples
自动验证 → tests
工程骨架 → templates
```

也就是说，实例项目是当前框架的一个目标应用形态，而 `multi-engine` 应沉淀出它背后的通用工程结构。

## 6. 总体架构

推荐整体分层如下：

```text
┌──────────────────────────────────────────────┐
│ Application Layer                             │
│ 具体应用、示例程序、业务工程                  │
│ minimal_app / game_server / simulation_app    │
└──────────────────────────────────────────────┘
                    ↓
┌──────────────────────────────────────────────┐
│ Domain Layer                                  │
│ 可选领域模块                                  │
│ simulation / world / entity / environment     │
│ data / editor / replay                        │
└──────────────────────────────────────────────┘
                    ↓
┌──────────────────────────────────────────────┐
│ Runtime Layer                                 │
│ 应用运行时、模块管理、服务注册、调度          │
│ runtime / scheduler                           │
└──────────────────────────────────────────────┘
                    ↓
┌──────────────────────────────────────────────┐
│ Common Capability Layer                       │
│ 通用工程能力                                  │
│ communication / plugin / logging / config     │
│ event / metrics / serialization / resource    │
└──────────────────────────────────────────────┘
                    ↓
┌──────────────────────────────────────────────┐
│ Base Layer                                    │
│ 基础类型、工具、生命周期接口                  │
│ baselib                                       │
└──────────────────────────────────────────────┘
                    ↓
┌──────────────────────────────────────────────┐
│ Thirdparty                                    │
│ asio / cyclonedds / spdlog / nlohmann_json    │
│ zlib / eigen                                  │
└──────────────────────────────────────────────┘
```

该分层的核心原则是：

- 应用层只组合能力，不直接承载框架基础逻辑。
- 领域层只表达业务模型，不直接污染通信、插件、配置等底层能力。
- 运行时层负责编排生命周期和模块依赖。
- 通用能力层提供稳定服务。
- 基础层保持最小依赖。

## 7. 推荐目录结构

最终推荐目录结构如下：

```text
multi-engine
├─ CMakeLists.txt
├─ cmake
│  ├─ options.cmake
│  ├─ dependencies.cmake
│  ├─ module.cmake
│  └─ install.cmake
│
├─ thirdparty
│  ├─ asio
│  ├─ cyclonedds
│  ├─ nlohmann_json
│  ├─ spdlog
│  ├─ zlib
│  └─ eigen
│
├─ modules
│  ├─ baselib
│  ├─ logging
│  ├─ config
│  ├─ event
│  ├─ metrics
│  ├─ serialization
│  ├─ communication
│  ├─ plugin
│  ├─ runtime
│  ├─ scheduler
│  ├─ resource
│  │
│  ├─ simulation
│  ├─ world
│  ├─ entity
│  ├─ environment
│  ├─ data
│  ├─ editor
│  └─ replay
│
├─ examples
│  ├─ minimal_app
│  ├─ communication_game_server
│  ├─ simulation_engine_app
│  ├─ plugin_demo
│  └─ scenario_editor_demo
│
├─ templates
│  ├─ module_template
│  ├─ plugin_template
│  ├─ app_template
│  └─ simulation_project_template
│
├─ tests
│  ├─ framework
│  ├─ communication
│  ├─ unit
│  ├─ integration
│  └─ benchmark
│
├─ configs
│  ├─ default_engine.json
│  ├─ default_simulation.json
│  ├─ default_logging.json
│  └─ default_communication.json
│
└─ docs
   ├─ architecture_design.md
   ├─ module_development.md
   ├─ plugin_development.md
   ├─ simulation_design.md
   └─ communication_design.md
```

当前项目还处在架构演进阶段，部分目录属于目标结构，并非都已实现。

## 8. 模块说明

### 8.1 baselib

`baselib` 是最底层基础库，所有模块都可以依赖它。

当前职责：

- 提供通用单例模板
- 提供 `IModule` 模块生命周期接口
- 提供 `ModuleState` 模块状态定义

后续可扩展：

- `Result<T>`
- `ErrorCode`
- 时间工具
- UUID/ID 生成
- 字节缓冲工具
- 线程安全队列
- noncopyable / scope guard

### 8.2 logging

`logging` 提供统一日志入口，封装 spdlog。

当前职责：

- 初始化全局 logger
- 控制台和文件日志输出
- 日志分类
- 日志级别控制
- 日志宏
- 基础日志统计

后续可扩展：

- 异步日志
- 性能埋点
- 审计日志
- 日志监听器
- 日志上下文链路追踪

### 8.3 config

`config` 提供统一配置读取能力。

当前职责：

- 加载 JSON 文件
- 加载 JSON 字符串
- 支持 `application.name` 形式的点路径访问
- 获取 string/int/double/bool/string array
- 获取配置片段 JSON

后续可扩展：

- 默认配置合并
- 命令行覆盖
- 环境变量覆盖
- 配置 schema 校验
- 配置热重载

### 8.4 event

`event` 是进程内事件总线，用于模块间解耦通信。

当前职责：

- 事件发布
- 事件订阅
- 通配订阅
- 事件统计

后续可扩展：

- 异步事件队列
- 多 worker 派发
- request-response
- 事件优先级
- 死信队列
- 与 `communication` 的网络事件桥接

### 8.5 scheduler

`scheduler` 提供主循环调度和事件调度。

当前职责：

- 注册可更新模块
- 按依赖和优先级执行主循环 step
- 定时事件
- 循环事件
- 条件事件

后续可扩展：

- 固定时间步
- 可变时间步
- FPS 统计
- 任务队列
- 线程池调度

### 8.6 runtime

`runtime` 是应用运行时层，负责编排模块和服务。

当前职责：

- `Application` 应用入口封装
- `EngineContext` 运行上下文
- `ServiceRegistry` 服务注册与查询
- `ModuleManager` 模块注册、初始化、启动、停止和关闭

推荐使用方式：

```text
main
  ↓
Application::Run
  ↓
configure callback
  ↓
ModuleManager::InitializeAll
  ↓
ModuleManager::StartAll
  ↓
main loop callback
  ↓
ModuleManager::StopAll
  ↓
ModuleManager::ShutdownAll
```

### 8.7 communication

`communication` 是跨进程、跨节点的通信模块。

当前职责：

- 多协议通信 Hub
- TCP/UDP/DDS 适配器
- 点对点发送
- 广播
- topic 发布订阅
- 客户端连接管理
- 协议适配器工厂
- 通信统计和错误回调

后续建议：

- 抽出 `MessageCodec`
- 抽出 `ProtocolRegistry`
- 增强消息路由
- 增加节点发现
- 增加 WebSocket/MQTT 适配器
- 与 `event` 做本地/网络事件桥接

### 8.8 plugin

`plugin` 是运行时扩展模块。

当前职责：

- 插件接口 `IPlugin`
- 插件基类 `PluginBase`
- 插件管理器 `IPluginManager`
- 动态库加载器 `IDynamicLibraryLoader`
- 插件加载、卸载、启动、停止
- 插件命令执行

后续建议：

- 插件 manifest
- 插件依赖拓扑排序
- 插件上下文 `PluginContext`
- 插件配置加载
- 插件权限边界

## 9. 仿真领域模块规划

仿真领域模块来自实例项目，目前建议作为后续可选模块逐步迁移。

### 9.1 simulation

负责仿真生命周期和仿真状态机。

规划能力：

- 仿真状态：未初始化、就绪、运行、暂停、停止、错误
- 仿真时间
- time scale
- pause/resume/step
- 仿真模块注册
- 仿真统计
- 场景加载/保存编排
- 快照编排

### 9.2 world

负责维护仿真世界状态。

规划能力：

- 实体状态
- 环境状态
- 态势事件
- COP 数据
- 世界快照
- 空间查询
- 相对关系计算
- 状态变更回调

### 9.3 entity

负责实体生命周期和实体行为更新。

规划能力：

- 实体创建/销毁
- 实体模板实例化
- ECS
- 实体命令处理
- 实体状态更新
- 向 `world` 提交状态

### 9.4 environment

负责环境仿真。

规划能力：

- 气象模型
- 海洋模型
- 声学模型
- 环境影响计算
- 环境覆盖
- 位置环境查询

### 9.5 data

负责仿真领域数据管理。

规划能力：

- 实体模板加载
- 场景数据加载
- 环境配置加载
- 数据缓存
- 数据索引
- 热重载

### 9.6 editor

负责想定编辑。

规划能力：

- 创建想定
- 加载想定
- 保存想定
- 添加实体
- 添加任务
- 设置环境
- 验证想定

### 9.7 replay

负责仿真回放。

规划能力：

- 回放帧记录
- 世界状态回放
- 事件回放
- 性能数据回放
- 回放导入导出

## 10. 模块依赖关系

推荐依赖关系：

```text
baselib
  ↓
logging / config / event / metrics / serialization / resource
  ↓
runtime / scheduler / communication / plugin
  ↓
simulation
  ↓
world / data / environment / entity / editor / replay
```

详细原则：

- `baselib` 不依赖其他内部模块。
- `logging/config/event` 是通用能力，可被上层模块使用。
- `runtime` 负责组合模块，不承载具体业务模型。
- `communication` 不定义仿真领域命令。
- `world/entity/environment/data/editor` 属于仿真领域，不应反向污染通用框架层。
- 示例应用负责组合模块，而不是定义基础能力。

## 11. 关键运行流程

### 11.1 应用启动流程

```text
main
  ↓
Application 创建 EngineContext
  ↓
初始化 ConfigService / Logger / EventBus
  ↓
注册服务到 ServiceRegistry
  ↓
注册模块到 ModuleManager
  ↓
InitializeAll
  ↓
StartAll
  ↓
MainLoop
  ↓
StopAll
  ↓
ShutdownAll
```

### 11.2 通信消息处理流程

```text
NetworkMessage
  ↓
CommunicationHub
  ↓
MessageReceiver
  ↓
业务消息解析 / CommandAdapter
  ↓
EventBus Publish
  ↓
业务模块处理
```

### 11.3 仿真主循环流程

```text
SimulationManager
  ↓
MainLoopScheduler::ExecuteStep
  ↓
Environment Update
  ↓
Entity Update
  ↓
WorldModel Update
  ↓
Snapshot / COP / Metrics / Communication Publish
```

### 11.4 插件加载流程

```text
PluginManager ScanPlugins
  ↓
读取插件 manifest
  ↓
检查依赖
  ↓
DynamicLibraryLoader Load
  ↓
CreatePlugin
  ↓
Plugin Initialize
  ↓
Plugin Start
```

## 12. 构建系统

当前顶层 CMake 已开始支持模块开关：

```cmake
option(ME_ENABLE_LOGGING "Enable logging module" ON)
option(ME_ENABLE_CONFIG "Enable config module" ON)
option(ME_ENABLE_EVENT "Enable event module" ON)
option(ME_ENABLE_SCHEDULER "Enable scheduler module" ON)
option(ME_ENABLE_RUNTIME "Enable runtime module" ON)
option(ME_ENABLE_PLUGIN "Enable plugin module" ON)
option(ME_ENABLE_COMMUNICATION "Enable communication module" ON)
```

后续推荐补充：

```cmake
option(ME_ENABLE_METRICS "Enable metrics module" ON)
option(ME_ENABLE_SERIALIZATION "Enable serialization module" ON)
option(ME_ENABLE_RESOURCE "Enable resource module" ON)

option(ME_ENABLE_SIMULATION "Enable simulation domain modules" ON)
option(ME_ENABLE_WORLD "Enable world model module" ON)
option(ME_ENABLE_ENTITY "Enable entity module" ON)
option(ME_ENABLE_ENVIRONMENT "Enable environment module" ON)
option(ME_ENABLE_DATA "Enable data module" ON)
option(ME_ENABLE_EDITOR "Enable editor module" OFF)
option(ME_ENABLE_REPLAY "Enable replay module" ON)
```

模块目录统一采用：

```text
modules/<module>
├─ CMakeLists.txt
├─ interface
├─ include
└─ src
```

其中：

- `interface` 放对外公开接口。
- `include` 放模块内部头文件。
- `src` 放实现文件。

## 13. 示例项目规划

示例项目用于说明框架如何组合使用，不应与自动化测试混淆。

推荐示例：

| 示例 | 说明 |
| --- | --- |
| `minimal_app` | 展示 runtime/config/logging/event 的最小用法 |
| `communication_game_server` | 展示 TCP/UDP 通信、主题消息和业务处理 |
| `simulation_engine_app` | 展示完整仿真引擎链路 |
| `plugin_demo` | 展示插件开发、加载和命令执行 |
| `scenario_editor_demo` | 展示想定编辑与数据管理 |

当前 `tests/communication/game_test` 更像示例项目，后续建议迁移到：

```text
examples/communication_game_server
```

## 14. 测试策略

测试目录应主要承载自动化验证。

当前已有：

```text
tests/framework/framework_smoke_test.cpp
```

该测试验证了：

- logging 初始化
- config JSON 读取
- event 发布订阅
- scheduler step 执行
- runtime 应用生命周期

后续建议补充：

- `tests/unit`：单模块单元测试
- `tests/integration`：多模块集成测试
- `tests/benchmark`：通信、调度、序列化性能测试
- `tests/plugin`：插件加载与卸载测试
- `tests/simulation`：仿真主循环和快照测试

## 15. 演进路线

### 第一阶段：框架底座

目标：完善通用工程能力。

当前已开始实现：

- `logging`
- `config`
- `event`
- `scheduler`
- `runtime`
- framework smoke test

下一步建议：

- 完善 `event` 异步队列
- 完善 `runtime` 模块依赖排序和错误处理
- 补充 `metrics`
- 补充 `serialization`
- 补充 `resource`

### 第二阶段：通信与插件增强

目标：让通信和插件成为稳定扩展点。

建议：

- 抽出 `MessageCodec`
- 抽出 `ProtocolRegistry`
- 为 communication 增加事件桥接
- 为 plugin 增加 manifest 和 PluginContext
- 增加插件 demo 和插件测试

### 第三阶段：仿真领域模块

目标：吸收实例项目中的仿真能力。

建议迁移：

```text
core/SimulationManager → modules/simulation
world                  → modules/world
data                   → modules/data
environment            → modules/environment
entity                 → modules/entity
editor                 → modules/editor
ReplayFrame            → modules/replay
```

迁移时应避免原样复制，而应先去除与底层通信、日志、序列化的耦合。

### 第四阶段：示例与模板

目标：让项目具备模板工程属性。

建议新增：

```text
examples/minimal_app
examples/communication_game_server
examples/simulation_engine_app
examples/plugin_demo
templates/module_template
templates/plugin_template
templates/app_template
templates/simulation_project_template
```

### 第五阶段：工程质量

目标：提升可维护性和交付质量。

建议：

- 增加 CI
- 增加 install/export
- 增加更完整测试
- 增加模块开发文档
- 增加插件开发文档
- 增加通信协议文档

## 16. 设计原则

项目后续演进应遵循以下原则：

1. **通用能力与领域能力分离**  
   通信、配置、日志、运行时等能力不应依赖仿真业务模型。

2. **模块间通过接口协作**  
   模块对外暴露 `interface`，内部实现放在 `include/src`。

3. **示例代码不进入框架核心**  
   完整业务示例应放在 `examples`，自动化验证放在 `tests`。

4. **通信不定义业务命令**  
   网络消息应转换为领域命令后再交给业务模块。

5. **运行时只负责编排**  
   `runtime` 不应承载具体仿真、实体、环境逻辑。

6. **数据模型不直接绑定序列化细节**  
   序列化、压缩、版本兼容应逐步下沉到独立模块。

7. **优先保证可构建、可测试、可扩展**  
   每新增一个模块，都应提供最小测试或示例。

## 17. 总结

`multi-engine` 的核心价值在于提供一套可复用的 C++ 多引擎工程底座。

当前项目已经从最初的通信和插件雏形，扩展出日志、配置、事件、调度和运行时等基础模块。后续应继续沿着“通用框架能力先稳定，仿真领域能力再迁移”的路线推进。

最终项目应形成如下结构：

```text
framework modules  提供稳定底座
 domain modules    提供可选领域能力
 examples          展示典型使用方式
 tests             保证自动化验证
 templates         支持快速派生项目
```

通过这种方式，`multi-engine` 可以同时服务于仿真引擎、分布式游戏服务器、数字孪生平台和其他多模块 C++ 应用。