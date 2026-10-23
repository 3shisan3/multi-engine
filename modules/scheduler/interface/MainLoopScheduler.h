/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        MainLoopScheduler.h
Version:     1.0
Author:      cjx
start date: 2026-06-10
Description: 主循环调度器接口
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-06-10      cjx        create

*****************************************************************/

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