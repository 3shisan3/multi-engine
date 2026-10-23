/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        plugin_demo_host.cpp
Description: 插件开发示例 —— 宿主程序

             演示宿主如何：
             1. 创建插件管理器
             2. 加载插件动态库
             3. 初始化并启动插件
             4. 向插件下发命令
             5. 停止并卸载插件

             运行前请确保 demo_plugin 动态库与本程序在同一目录。

*****************************************************************/

#include "PluginManager.h"

#include <any>
#include <iostream>
#include <map>
#include <string>

using namespace PluginSystem;

namespace
{

std::string AnyToString(const std::any& value)
{
    if (!value.has_value())
    {
        return "(empty)";
    }
    try
    {
        return std::any_cast<std::string>(value);
    }
    catch (...)
    {
        // 非字符串类型，尝试数值
        try
        {
            return std::to_string(std::any_cast<int>(value));
        }
        catch (...)
        {
            return "(unsupported)";
        }
    }
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "usage: plugin_demo_host <demo_plugin library path>" << std::endl;
        return 2;
    }

    const std::string pluginPath = argv[1];

    auto manager = CreatePluginManager();
    manager->Initialize("configs/plugin"); // 配置目录（不存在时会创建默认配置）

    // 1. 加载插件（内部会完成 Initialize）
    std::string pluginId = manager->LoadPlugin(pluginPath);
    if (pluginId.empty())
    {
        std::cerr << "load plugin failed" << std::endl;
        return 3;
    }

    auto plugin = manager->GetPlugin(pluginId);

    // 2. 启动插件
    if (!manager->StartPlugin(pluginId))
    {
        std::cerr << "start plugin failed" << std::endl;
        return 4;
    }

    // 3. 下发命令
    std::cout << "plugin status: " << plugin->GetStatus() << std::endl;

    std::map<std::string, std::any> params;
    params["name"] = std::string("multi-engine");

    std::cout << "greet: " << AnyToString(plugin->ExecuteCommand("greet", params)) << std::endl;
    std::cout << "greet: " << AnyToString(plugin->ExecuteCommand("greet", params)) << std::endl;
    std::cout << "version: " << AnyToString(plugin->ExecuteCommand("version", {})) << std::endl;
    std::cout << "stats: " << AnyToString(plugin->ExecuteCommand("stats", {})) << std::endl;

    // 4. 停止并卸载
    manager->StopPlugin(pluginId);
    if (!manager->UnloadPlugin(pluginId))
    {
        std::cerr << "unload plugin failed" << std::endl;
        return 5;
    }

    std::cout << "plugin_demo_host finished" << std::endl;
    return 0;
}
