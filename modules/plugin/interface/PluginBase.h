/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        PluginBase.h
Version:     1.0
Author:      cjx
start date: 2026-1-11
Description: 插件基类，提供常用功能
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-1-10      cjx            create
*****************************************************************/

#ifndef PLUGIN_SYSTEM_BASE_H
#define PLUGIN_SYSTEM_BASE_H

#include "PluginInterface.h"

#include <atomic>
#include <map>
#include <mutex>
#include <string>

namespace PluginSystem
{

/**
 * @brief 插件基类
 * 提供插件的基本实现，具体插件可继承此类
 */
class PluginBase : public IPlugin
{
public:
    PluginBase(const std::string &pluginId,
               const std::string &pluginType,
               const std::string &pluginName = "",
               const std::string &version = "1.0.0");
    virtual ~PluginBase();

    // IPlugin 接口实现
    PluginInfo GetPluginInfo() const override;
    bool Initialize(const std::map<std::string, std::any> &config) override;
    bool Start() override;
    void Stop() override;
    void Unload() override;
    std::string GetStatus() const override;
    std::any ExecuteCommand(const std::string &command,
                            const std::map<std::string, std::any> &params) override;
    std::string GetPluginType() const override;

    // 扩展功能
    void SetPluginInfo(const PluginInfo &info);
    void AddDependency(const std::string &dependency);
    void SetDescription(const std::string &description);
    void SetAuthor(const std::string &author);
    void SetLicense(const std::string &license);

    void SetLogger(std::function<void(const std::string &, int)> logger);
    void LogInfo(const std::string &message) const;
    void LogWarning(const std::string &message) const;
    void LogError(const std::string &message) const;

    void SetEventCallback(PluginEventCallback callback);
    void EmitEvent(PluginEvent event, const std::any &data = std::any());

    bool IsInitialized() const { return isInitialized_; }
    bool IsRunning() const { return isRunning_; }

    const std::map<std::string, std::any> &GetConfig() const { return config_; }

    template <typename T>
    T GetConfigValue(const std::string &key, const T &defaultValue = T()) const;

protected:
    // 子类需要实现的方法
    virtual bool OnInitialize(const std::map<std::string, std::any> &config) { return true; }
    virtual bool OnStart() { return true; }
    virtual void OnStop() {}
    virtual void OnUnload() {}
    virtual std::any OnExecuteCommand(const std::string &command,
                                      const std::map<std::string, std::any> &params) { return std::any(); }

    void UpdateConfig(const std::map<std::string, std::any> &newConfig);

private:
    PluginInfo info_;
    std::map<std::string, std::any> config_;
    std::atomic<bool> isInitialized_{false};
    std::atomic<bool> isRunning_{false};

    std::function<void(const std::string &, int)> logger_;
    PluginEventCallback eventCallback_;

    mutable std::mutex configMutex_;
};

} // namespace PluginSystem

#endif // PLUGIN_SYSTEM_BASE_H