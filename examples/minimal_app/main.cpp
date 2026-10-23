/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        main.cpp
Description: 最小应用示例

             展示如何用 runtime 组合 logging / config / event / metrics
             完成一次完整的应用生命周期。

*****************************************************************/

#include "Application.h"
#include "ConfigService.h"
#include "EventBus.h"
#include "Logger.h"
#include "Metrics.h"
#include "module_interface.h"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace RuntimeModule;

namespace
{

/**
 * @brief 示例业务模块
 *
 * 演示 IModule 的生命周期实现，以及通过 GetDependencies 声明依赖，
 * 由 ModuleManager 自动做拓扑排序。
 */
class DemoModule : public baselib::IModule
{
public:
    explicit DemoModule(std::string name, std::vector<std::string> deps = {})
        : name_(std::move(name)), deps_(std::move(deps)) {}

    std::string GetName() const override { return name_; }
    std::string GetVersion() const override { return "1.0.0"; }
    std::vector<std::string> GetDependencies() const override { return deps_; }

    bool Initialize(const std::string& config = "") override
    {
        (void)config;
        state_ = baselib::ModuleState::Initialized;
        std::cout << "[DemoModule] " << name_ << " initialized" << std::endl;
        return true;
    }

    bool Start() override
    {
        state_ = baselib::ModuleState::Running;
        std::cout << "[DemoModule] " << name_ << " started" << std::endl;
        return true;
    }

    void Stop() override
    {
        state_ = baselib::ModuleState::Stopped;
        std::cout << "[DemoModule] " << name_ << " stopped" << std::endl;
    }

    void Shutdown() override
    {
        state_ = baselib::ModuleState::Stopped;
        std::cout << "[DemoModule] " << name_ << " shutdown" << std::endl;
    }

    baselib::ModuleState GetState() const override { return state_; }

private:
    std::string name_;
    std::vector<std::string> deps_;
    baselib::ModuleState state_ = baselib::ModuleState::Created;
};

} // namespace

int main()
{
    // 1. 初始化日志
    if (!LogModule::Logger::InitializeDefault("logs"))
    {
        std::cerr << "logger init failed" << std::endl;
        return 1;
    }
    ME_LOG_INFO(LogModule::LogCategory::SYSTEM, "minimal_app", "logger ready");

    // 2. 读取配置
    auto config = ConfigModule::CreateConfigService();
    if (!config->LoadFile("configs/default_engine.json"))
    {
        std::cerr << "config load failed: " << config->GetLastError() << std::endl;
        return 2;
    }
    ME_LOG_INFO(LogModule::LogCategory::SYSTEM, "minimal_app",
                "app name = {}", config->GetString("application.name"));

    // 3. 指标统计
    auto metrics = MetricsModule::CreateMetrics();
    metrics->Gauge("demo.workers", 4);

    // 4. 事件总线
    auto eventBus = EventModule::CreateEventBus();
    eventBus->Start();
    int received = 0;
    eventBus->Subscribe("demo.tick", [&](const EventModule::Event&) { ++received; });

    // 5. 组装应用：注册服务与模块（模块 B 依赖模块 A）
    Application app;

    auto moduleA = std::make_shared<DemoModule>("module_a");
    auto moduleB = std::make_shared<DemoModule>("module_b", std::vector<std::string>{"module_a"});

    app.SetConfigureCallback([&](EngineContext& context) {
        RegisterTypedService(context.Services(), "config", config);
        context.Modules().RegisterModule(moduleA);
        context.Modules().RegisterModule(moduleB);
        return true;
    });

    // 6. 主循环：模拟运行 3 帧
    app.SetMainLoopCallback([&](EngineContext&) {
        for (int frame = 0; frame < 3; ++frame)
        {
            eventBus->Publish("demo.tick");
            metrics->Increment("demo.loop");
            metrics->Observe("demo.frame.ms", 16.6);
        }
        return 0;
    });

    int result = app.Run();

    ME_LOG_INFO(LogModule::LogCategory::SYSTEM, "minimal_app",
                "events received = {}, loop count = {}",
                received,
                metrics->Get("demo.loop").counter);

    LogModule::Logger::GetInstance().Shutdown();

    std::cout << "minimal_app finished with code " << result << std::endl;
    return result;
}
