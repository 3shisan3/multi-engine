/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        sample_plugin.cpp
Description: 插件模块冒烟测试用的示例插件
             演示如何实现 IPlugin 并通过导出函数被 PluginManager 加载

*****************************************************************/

#include "PluginInterface.h"

#include <any>
#include <map>
#include <string>

#ifdef _WIN32
    #define SAMPLE_PLUGIN_EXPORT __declspec(dllexport)
#else
    #define SAMPLE_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

namespace
{

class SamplePlugin : public PluginSystem::IPlugin
{
public:
    SamplePlugin()
    {
        info_.pluginId = "sample_plugin";
        info_.pluginName = "SamplePlugin";
        info_.pluginVersion = "1.0.0";
        info_.pluginType = "demo";
        info_.description = "plugin smoke test sample";
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

        auto it = config.find("greeting");
        if (it != config.end())
        {
            try
            {
                greeting_ = std::any_cast<std::string>(it->second);
            }
            catch (...)
            {
                greeting_ = "hello";
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
        (void)params;

        if (command == "echo")
        {
            return greeting_;
        }
        if (command == "status")
        {
            return GetStatus();
        }
        if (command == "ping")
        {
            return std::string("pong");
        }
        return std::any();
    }

    std::string GetPluginType() const override
    {
        return "demo";
    }

private:
    PluginSystem::PluginInfo info_;
    bool initialized_ = false;
    bool running_ = false;
    std::string greeting_ = "hello";
};

} // namespace

extern "C" SAMPLE_PLUGIN_EXPORT PluginSystem::IPlugin* CreatePlugin()
{
    return new SamplePlugin();
}

extern "C" SAMPLE_PLUGIN_EXPORT void DestroyPlugin(PluginSystem::IPlugin* plugin)
{
    delete plugin;
}
