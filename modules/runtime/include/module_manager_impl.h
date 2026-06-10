#ifndef ME_RUNTIME_MODULE_MANAGER_IMPL_H
#define ME_RUNTIME_MODULE_MANAGER_IMPL_H

#include "ModuleManager.h"

#include <map>
#include <mutex>

namespace RuntimeModule
{

class ModuleManagerImpl : public IModuleManager
{
public:
    bool RegisterModule(std::shared_ptr<baselib::IModule> module) override;
    bool UnregisterModule(const std::string& name) override;
    std::shared_ptr<baselib::IModule> GetModule(const std::string& name) const override;
    std::vector<std::string> GetModuleNames() const override;

    bool InitializeAll(const std::string& config = "") override;
    bool StartAll() override;
    void StopAll() override;
    void ShutdownAll() override;

private:
    std::vector<std::string> BuildExecutionOrder() const;

    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<baselib::IModule>> modules_;
    std::vector<std::string> executionOrder_;
};

} // namespace RuntimeModule

#endif // ME_RUNTIME_MODULE_MANAGER_IMPL_H