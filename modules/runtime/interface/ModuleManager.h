#ifndef ME_RUNTIME_MODULE_MANAGER_H
#define ME_RUNTIME_MODULE_MANAGER_H

#include "module_interface.h"

#include <memory>
#include <string>
#include <vector>

namespace RuntimeModule
{

class IModuleManager
{
public:
    virtual ~IModuleManager() = default;

    virtual bool RegisterModule(std::shared_ptr<baselib::IModule> module) = 0;
    virtual bool UnregisterModule(const std::string& name) = 0;
    virtual std::shared_ptr<baselib::IModule> GetModule(const std::string& name) const = 0;
    virtual std::vector<std::string> GetModuleNames() const = 0;

    virtual bool InitializeAll(const std::string& config = "") = 0;
    virtual bool StartAll() = 0;
    virtual void StopAll() = 0;
    virtual void ShutdownAll() = 0;
};

std::shared_ptr<IModuleManager> CreateModuleManager();

} // namespace RuntimeModule

#endif // ME_RUNTIME_MODULE_MANAGER_H