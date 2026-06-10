#include "module_manager_impl.h"

#include <functional>
#include <set>

namespace RuntimeModule
{

bool ModuleManagerImpl::RegisterModule(std::shared_ptr<baselib::IModule> module)
{
    if (!module || module->GetName().empty())
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto name = module->GetName();
    if (modules_.find(name) != modules_.end())
    {
        return false;
    }
    modules_[name] = std::move(module);
    executionOrder_.clear();
    return true;
}

bool ModuleManagerImpl::UnregisterModule(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto removed = modules_.erase(name) > 0;
    if (removed)
    {
        executionOrder_.clear();
    }
    return removed;
}

std::shared_ptr<baselib::IModule> ModuleManagerImpl::GetModule(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = modules_.find(name);
    return it == modules_.end() ? nullptr : it->second;
}

std::vector<std::string> ModuleManagerImpl::GetModuleNames() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> names;
    for (const auto& pair : modules_)
    {
        names.push_back(pair.first);
    }
    return names;
}

bool ModuleManagerImpl::InitializeAll(const std::string& config)
{
    std::vector<std::shared_ptr<baselib::IModule>> modules;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        executionOrder_ = BuildExecutionOrder();
        if (executionOrder_.size() != modules_.size())
        {
            return false;
        }
        for (const auto& name : executionOrder_)
        {
            modules.push_back(modules_.at(name));
        }
    }

    for (const auto& module : modules)
    {
        if (!module->Initialize(config))
        {
            return false;
        }
    }
    return true;
}

bool ModuleManagerImpl::StartAll()
{
    std::vector<std::shared_ptr<baselib::IModule>> modules;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& name : executionOrder_)
        {
            modules.push_back(modules_.at(name));
        }
    }

    for (const auto& module : modules)
    {
        if (!module->Start())
        {
            return false;
        }
    }
    return true;
}

void ModuleManagerImpl::StopAll()
{
    std::vector<std::shared_ptr<baselib::IModule>> modules;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = executionOrder_.rbegin(); it != executionOrder_.rend(); ++it)
        {
            modules.push_back(modules_.at(*it));
        }
    }

    for (const auto& module : modules)
    {
        module->Stop();
    }
}

void ModuleManagerImpl::ShutdownAll()
{
    std::vector<std::shared_ptr<baselib::IModule>> modules;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = executionOrder_.rbegin(); it != executionOrder_.rend(); ++it)
        {
            modules.push_back(modules_.at(*it));
        }
    }

    for (const auto& module : modules)
    {
        module->Shutdown();
    }
}

std::vector<std::string> ModuleManagerImpl::BuildExecutionOrder() const
{
    std::vector<std::string> order;
    std::set<std::string> visited;
    std::set<std::string> visiting;

    std::function<bool(const std::string&)> visit = [&](const std::string& name) {
        if (visited.count(name))
        {
            return true;
        }
        if (visiting.count(name))
        {
            return false;
        }

        auto it = modules_.find(name);
        if (it == modules_.end())
        {
            return false;
        }

        visiting.insert(name);
        for (const auto& dependency : it->second->GetDependencies())
        {
            if (modules_.find(dependency) != modules_.end() && !visit(dependency))
            {
                return false;
            }
        }
        visiting.erase(name);
        visited.insert(name);
        order.push_back(name);
        return true;
    };

    for (const auto& pair : modules_)
    {
        if (!visit(pair.first))
        {
            return {};
        }
    }

    return order;
}

std::shared_ptr<IModuleManager> CreateModuleManager()
{
    return std::make_shared<ModuleManagerImpl>();
}

} // namespace RuntimeModule