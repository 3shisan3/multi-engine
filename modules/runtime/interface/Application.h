#ifndef ME_RUNTIME_APPLICATION_H
#define ME_RUNTIME_APPLICATION_H

#include "EngineContext.h"

#include <functional>
#include <memory>
#include <string>

namespace RuntimeModule
{

class Application
{
public:
    using ConfigureCallback = std::function<bool(EngineContext&)>;
    using MainLoopCallback = std::function<int(EngineContext&)>;

    Application();

    void SetConfigureCallback(ConfigureCallback callback);
    void SetMainLoopCallback(MainLoopCallback callback);
    int Run();
    EngineContext& GetContext();

private:
    EngineContext context_;
    ConfigureCallback configureCallback_;
    MainLoopCallback mainLoopCallback_;
};

} // namespace RuntimeModule

#endif // ME_RUNTIME_APPLICATION_H