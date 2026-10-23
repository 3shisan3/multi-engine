/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        EngineContext.h
Version:     1.0
Author:      cjx
start date: 2026-06-10
Description: 引擎运行上下文接口
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-06-10      cjx        create

*****************************************************************/

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