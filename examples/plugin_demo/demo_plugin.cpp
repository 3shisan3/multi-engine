/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        demo_plugin.cpp
Description: 插件开发示例 —— 业务插件

             演示：
             1. 如何实现 IPlugin 接口
             2. 如何在 Initialize 中读取配置
             3. 如何在 ExecuteCommand 中响应宿主命令
             4. 如何导出 CreatePlugin / DestroyPlugin

             编译产物为动态库，由宿主在运行时加载。

*****************************************************************/

#include "PluginInterface.h"

#include <any>
#include <map>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
    #define DEMO_PLUGIN_EXPORT __declspec(dllexport)
#else
    #define DEMO_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

namespace
{

class GreeterPlugin : public PluginSystem::IPlugin
{
public:
    GreeterPlugin()
    {
        info_.pluginId = "greeter";
        info_.pluginName = "Greeter";
        info_.pluginVersion = "1.0.0";
        info_.pluginType = "business";
        info_.description = "示例业务插件：提供问候与统计命令";
        info_.author = "multi-engine";
    }

    PluginSystem::PluginInfo GetPluginInfo() const override
    {
        return info_;
    }

    bool Initialize(const std::map<std::string, std::any>& config) override
    {
        if (initialized_)
        {
            return true;
        }

        // 从配置读取前缀，缺省为 "Hello"
        auto it = config.find("prefix");
        if (it != config.end())
        {
            try
            {
                prefix_ = std::any_cast<std::string>(it->second);
            }
            catch (...)
            {
                prefix_ = "Hello";
            }
        }

        initialized_ = true;
        return true;
    }

    bool Start() override
    {
        if (!initialized_)
        {
            return false;
        }
        running_ = true;
        return true;
    }

    void Stop() override
    {
        running_ = false;
    }

    void Unload() override
    {
        Stop();
        initialized_ = false;
    }

    std::string GetStatus() const override
    {
        if (running_)
        {
            return "running";
        }
        return initialized_ ? "initialized" : "created";
    }

    std::any ExecuteCommand(const std::string& command,
                            const std::map<std::string, std::any>& params) override
    {
        if (command == "greet")
        {
            std::string who = "world";
            auto it = params.find("name");
            if (it != params.end())
            {
                try
                {
                    who = std::any_cast<std::string>(it->second);
                }
                catch (...)
                {
                    // 保持默认值
                }
            }

            std::string message = prefix_ + ", " + who + "!";
            greetCount_++;
            return message;
        }

        if (command == "stats")
        {
            return greetCount_;
        }

        if (command == "version")
        {
            return info_.pluginVersion;
        }

        return std::any();
    }

    std::string GetPluginType() const override
    {
        return info_.pluginType;
    }

private:
    PluginSystem::PluginInfo info_;
    bool initialized_ = false;
    bool running_ = false;
    std::string prefix_ = "Hello";
    int greetCount_ = 0;
};

} // namespace

extern "C" DEMO_PLUGIN_EXPORT PluginSystem::IPlugin* CreatePlugin()
{
    return new GreeterPlugin();
}

extern "C" DEMO_PLUGIN_EXPORT void DestroyPlugin(PluginSystem::IPlugin* plugin)
{
    delete plugin;
}
