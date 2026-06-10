#ifndef ME_RUNTIME_ENGINE_CONTEXT_H
#define ME_RUNTIME_ENGINE_CONTEXT_H

#include "ModuleManager.h"
#include "ServiceRegistry.h"

#include <memory>

namespace RuntimeModule
{

class EngineContext
{
public:
    EngineContext();

    IServiceRegistry& Services();
    IModuleManager& Modules();

private:
    std::shared_ptr<IServiceRegistry> serviceRegistry_;
    std::shared_ptr<IModuleManager> moduleManager_;
};

} // namespace RuntimeModule

#endif // ME_RUNTIME_ENGINE_CONTEXT_H