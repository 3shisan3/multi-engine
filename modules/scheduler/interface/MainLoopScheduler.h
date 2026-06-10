#ifndef ME_SCHEDULER_MAIN_LOOP_SCHEDULER_H
#define ME_SCHEDULER_MAIN_LOOP_SCHEDULER_H

#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace SchedulerModule
{

using UpdateFunction = std::function<void(double)>;

struct ModuleInfo
{
    std::string name;
    UpdateFunction updateFunc;
    int priority = 0;
    std::vector<std::string> dependencies;
    bool enabled = true;
};

class MainLoopScheduler
{
public:
    bool RegisterModule(const ModuleInfo& moduleInfo);
    bool UnregisterModule(const std::string& moduleName);
    void ExecuteStep(double deltaTime);
    bool BuildExecutionOrder();
    std::vector<std::string> GetExecutionOrder() const;

private:
    std::vector<std::string> TopologicalSort() const;

    mutable std::mutex mutex_;
    std::map<std::string, ModuleInfo> modules_;
    std::vector<std::string> executionOrder_;
};

} // namespace SchedulerModule

#endif // ME_SCHEDULER_MAIN_LOOP_SCHEDULER_H