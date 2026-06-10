#ifndef ME_BASELIB_MODULE_INTERFACE_H
#define ME_BASELIB_MODULE_INTERFACE_H

#include <memory>
#include <string>
#include <vector>

namespace baselib
{

enum class ModuleState
{
    Created,
    Initialized,
    Running,
    Stopping,
    Stopped,
    Error
};

class IModule
{
public:
    virtual ~IModule() = default;

    virtual std::string GetName() const = 0;
    virtual std::string GetVersion() const = 0;
    virtual std::vector<std::string> GetDependencies() const = 0;

    virtual bool Initialize(const std::string& config = "") = 0;
    virtual bool Start() = 0;
    virtual void Stop() = 0;
    virtual void Shutdown() = 0;

    virtual ModuleState GetState() const = 0;
};

} // namespace baselib

#endif // ME_BASELIB_MODULE_INTERFACE_H