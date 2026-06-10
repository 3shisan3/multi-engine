#include "Application.h"

namespace RuntimeModule
{

Application::Application() = default;

void Application::SetConfigureCallback(ConfigureCallback callback)
{
    configureCallback_ = std::move(callback);
}

void Application::SetMainLoopCallback(MainLoopCallback callback)
{
    mainLoopCallback_ = std::move(callback);
}

int Application::Run()
{
    if (configureCallback_ && !configureCallback_(context_))
    {
        return 1;
    }

    if (!context_.Modules().InitializeAll())
    {
        return 2;
    }

    if (!context_.Modules().StartAll())
    {
        context_.Modules().ShutdownAll();
        return 3;
    }

    int result = mainLoopCallback_ ? mainLoopCallback_(context_) : 0;

    context_.Modules().StopAll();
    context_.Modules().ShutdownAll();
    return result;
}

EngineContext& Application::GetContext()
{
    return context_;
}

} // namespace RuntimeModule