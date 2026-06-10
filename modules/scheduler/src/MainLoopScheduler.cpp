#include "MainLoopScheduler.h"

#include <algorithm>
#include <set>

namespace SchedulerModule
{

bool MainLoopScheduler::RegisterModule(const ModuleInfo& moduleInfo)
{
    if (moduleInfo.name.empty() || !moduleInfo.updateFunc)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (modules_.find(moduleInfo.name) != modules_.end())
    {
        return false;
    }
    modules_[moduleInfo.name] = moduleInfo;
    executionOrder_.clear();
    return true;
}

bool MainLoopScheduler::UnregisterModule(const std::string& moduleName)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto removed = modules_.erase(moduleName) > 0;
    if (removed)
    {
        executionOrder_.clear();
    }
    return removed;
}

void MainLoopScheduler::ExecuteStep(double deltaTime)
{
    std::vector<ModuleInfo> modules;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (executionOrder_.empty())
        {
            executionOrder_ = TopologicalSort();
        }
        for (const auto& name : executionOrder_)
        {
            auto it = modules_.find(name);
            if (it != modules_.end() && it->second.enabled)
            {
                modules.push_back(it->second);
            }
        }
    }

    for (const auto& module : modules)
    {
        module.updateFunc(deltaTime);
    }
}

bool MainLoopScheduler::BuildExecutionOrder()
{
    std::lock_guard<std::mutex> lock(mutex_);
    executionOrder_ = TopologicalSort();
    return executionOrder_.size() == modules_.size();
}

std::vector<std::string> MainLoopScheduler::GetExecutionOrder() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return executionOrder_;
}

std::vector<std::string> MainLoopScheduler::TopologicalSort() const
{
    std::vector<ModuleInfo> sortedModules;
    for (const auto& pair : modules_)
    {
        sortedModules.push_back(pair.second);
    }

    std::sort(sortedModules.begin(), sortedModules.end(), [](const ModuleInfo& lhs, const ModuleInfo& rhs) {
        return lhs.priority > rhs.priority;
    });

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
        for (const auto& dependency : it->second.dependencies)
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

    for (const auto& module : sortedModules)
    {
        if (!visit(module.name))
        {
            return {};
        }
    }

    return order;
}

} // namespace SchedulerModule