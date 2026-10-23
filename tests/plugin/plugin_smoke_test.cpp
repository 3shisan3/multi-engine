/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        plugin_smoke_test.cpp
Description: 插件模块冒烟测试宿主程序
             验证插件的加载、启动、命令执行、停止与卸载全链路

*****************************************************************/

#include "PluginManager.h"

#include <any>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "usage: plugin_smoke_test <plugin-library-path>" << std::endl;
        return 2;
    }

    const std::string pluginPath = argv[1];

    auto manager = PluginSystem::CreatePluginManager();
    if (!manager)
    {
        std::cerr << "failed to create plugin manager" << std::endl;
        return 3;
    }

    // 初始化（空配置目录，仅验证核心加载链路）
    if (!manager->Initialize(""))
    {
        std::cerr << "failed to initialize plugin manager" << std::endl;
        return 4;
    }

    // 1. 加载插件
    const std::string pluginId = manager->LoadPlugin(pluginPath);
    if (pluginId.empty())
    {
        std::cerr << "failed to load plugin: " << pluginPath << std::endl;
        return 5;
    }
    std::cout << "loaded plugin: " << pluginId << std::endl;

    auto plugin = manager->GetPlugin(pluginId);
    if (!plugin)
    {
        std::cerr << "plugin instance not found after load" << std::endl;
        return 6;
    }

    PluginSystem::PluginInfo info = plugin->GetPluginInfo();
    if (info.pluginName != "SamplePlugin" || info.pluginType != "demo")
    {
        std::cerr << "unexpected plugin info: " << info.pluginName << std::endl;
        return 7;
    }

    // 2. 启动插件
    if (!manager->StartPlugin(pluginId))
    {
        std::cerr << "failed to start plugin" << std::endl;
        return 8;
    }
    if (plugin->GetStatus() != "running")
    {
        std::cerr << "plugin not running after start" << std::endl;
        return 9;
    }

    // 3. 命令执行
    std::any echo = plugin->ExecuteCommand("echo", {});
    if (std::any_cast<std::string>(echo) != "hello")
    {
        std::cerr << "unexpected echo result" << std::endl;
        return 10;
    }

    std::any ping = plugin->ExecuteCommand("ping", {});
    if (std::any_cast<std::string>(ping) != "pong")
    {
        std::cerr << "unexpected ping result" << std::endl;
        return 11;
    }

    // 4. 停止插件
    manager->StopPlugin(pluginId);
    if (plugin->GetStatus() == "running")
    {
        std::cerr << "plugin still running after stop" << std::endl;
        return 12;
    }

    // 5. 卸载插件
    if (!manager->UnloadPlugin(pluginId))
    {
        std::cerr << "failed to unload plugin" << std::endl;
        return 13;
    }
    if (!manager->GetLoadedPlugins().empty())
    {
        std::cerr << "plugin list not empty after unload" << std::endl;
        return 14;
    }

    std::cout << "plugin smoke test passed" << std::endl;
    return 0;
}
