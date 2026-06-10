#include "EngineContext.h"

namespace RuntimeModule
{

EngineContext::EngineContext()
    : serviceRegistry_(CreateServiceRegistry()), moduleManager_(CreateModuleManager())
{
}

IServiceRegistry& EngineContext::Services()
{
    return *serviceRegistry_;
}

IModuleManager& EngineContext::Modules()
{
    return *moduleManager_;
}

} // namespace RuntimeModule