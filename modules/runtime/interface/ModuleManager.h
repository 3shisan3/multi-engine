/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        ModuleManager.h
Version:     1.0
Author:      cjx
start date: 2026-06-10
Description: 模块管理器接口
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-06-10      cjx        create

*****************************************************************/

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