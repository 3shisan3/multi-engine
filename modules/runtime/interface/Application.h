/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        Application.h
Version:     1.0
Author:      cjx
start date: 2026-06-10
Description: 应用入口接口
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-06-10      cjx        create

*****************************************************************/

#ifndef ME_RUNTIME_APPLICATION_H
#define ME_RUNTIME_APPLICATION_H

#include "EngineContext.h"

#include <functional>
#include <memory>
#include <string>

namespace RuntimeModule
{

class Application
{
public:
    using ConfigureCallback = std::function<bool(EngineContext&)>;
    using MainLoopCallback = std::function<int(EngineContext&)>;

    Application();

    void SetConfigureCallback(ConfigureCallback callback);
    void SetMainLoopCallback(MainLoopCallback callback);
    int Run();
    EngineContext& GetContext();

private:
    EngineContext context_;
    ConfigureCallback configureCallback_;
    MainLoopCallback mainLoopCallback_;
};

} // namespace RuntimeModule

#endif // ME_RUNTIME_APPLICATION_H