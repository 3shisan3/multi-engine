#include "Application.h"
#include "ConfigService.h"
#include "EventBus.h"
#include "Logger.h"
#include "MainLoopScheduler.h"
#include "module_interface.h"

#include <iostream>
#include <memory>
#include <string>

class SmokeModule : public baselib::IModule
{
public:
    std::string GetName() const override { return "smoke"; }
    std::string GetVersion() const override { return "1.0.0"; }
    std::vector<std::string> GetDependencies() const override { return {}; }

    bool Initialize(const std::string& config = "") override
    {
        state_ = baselib::ModuleState::Initialized;
        return true;
    }

    bool Start() override
    {
        state_ = baselib::ModuleState::Running;
        return true;
    }

    void Stop() override
    {
        state_ = baselib::ModuleState::Stopped;
    }

    void Shutdown() override
    {
        shutdown_ = true;
    }

    baselib::ModuleState GetState() const override { return state_; }
    bool IsShutdown() const { return shutdown_; }

private:
    baselib::ModuleState state_ = baselib::ModuleState::Created;
    bool shutdown_ = false;
};

int main()
{
    if (!LogModule::Logger::InitializeDefault("logs"))
    {
        return 1;
    }
    ME_LOG_INFO(LogModule::LogCategory::SYSTEM, "framework_smoke_test", "logger initialized");

    auto config = ConfigModule::CreateConfigService();
    if (!config->LoadString(R"({"application":{"name":"smoke"},"value":42,"enabled":true})"))
    {
        return 2;
    }
    if (config->GetString("application.name") != "smoke" || config->GetInt("value") != 42 || !config->GetBool("enabled"))
    {
        return 3;
    }

    auto eventBus = EventModule::CreateEventBus();
    int eventCount = 0;
    eventBus->Start();
    eventBus->Subscribe("smoke.event", [&](const EventModule::Event&) { ++eventCount; });
    eventBus->Publish("smoke.event");
    if (eventCount != 1)
    {
        return 4;
    }

    SchedulerModule::MainLoopScheduler scheduler;
    int updateCount = 0;
    scheduler.RegisterModule({"updater", [&](double) { ++updateCount; }, 0, {}, true});
    scheduler.ExecuteStep(0.016);
    if (updateCount != 1)
    {
        return 5;
    }

    RuntimeModule::Application app;
    auto smokeModule = std::make_shared<SmokeModule>();
    app.SetConfigureCallback([&](RuntimeModule::EngineContext& context) {
        return context.Modules().RegisterModule(smokeModule);
    });
    app.SetMainLoopCallback([&](RuntimeModule::EngineContext&) {
        return smokeModule->GetState() == baselib::ModuleState::Running ? 0 : 6;
    });

    auto result = app.Run();
    if (result != 0 || !smokeModule->IsShutdown())
    {
        return 7;
    }

    LogModule::Logger::GetInstance().Shutdown();
    std::cout << "framework smoke test passed" << std::endl;
    return 0;
}
