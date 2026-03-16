/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        plugin_manager_impl.h
Version:     1.0
Author:      cjx
start date: 2026-1-11
Description: 插件管理器实现类（内部）
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-1-10      cjx            create
*****************************************************************/

#ifndef PLUGIN_SYSTEM_MANAGER_IMPL_H
#define PLUGIN_SYSTEM_MANAGER_IMPL_H

#include "PluginManager.h"
#include "DynamicLibraryLoader.h"

#include <atomic>
#include <filesystem>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace PluginSystem
{

/**
 * @brief 插件管理器实现类
 * 
 * 管理插件的加载、卸载、启动、停止等生命周期操作。
 * 支持插件依赖关系检查和配置管理。
 */
class PluginManagerImpl : public IPluginManager
{
public:
    PluginManagerImpl();
    ~PluginManagerImpl() override;

    // IPluginManager 接口实现
    bool Initialize(const std::string &configDir = "") override;
    std::string LoadPlugin(const std::string &pluginPath) override;
    bool UnloadPlugin(const std::string &pluginId) override;
    std::shared_ptr<IPlugin> GetPlugin(const std::string &pluginId) const override;
    std::vector<PluginInfo> GetAllPluginInfo() const override;
    std::vector<std::string> GetLoadedPlugins() const override;
    int ScanPlugins(const std::string &pluginDir) override;
    bool StartAllPlugins() override;
    void StopAllPlugins() override;
    bool StartPlugin(const std::string &pluginId) override;
    void StopPlugin(const std::string &pluginId) override;
    bool RegisterPluginFactory(const std::string &pluginType,
                               std::function<std::shared_ptr<IPlugin>()> factory) override;
    void SetPluginEventCallback(PluginEventCallback callback) override;
    std::map<std::string, std::any> GetStatus() const override;
    bool CheckDependencies(const std::string &pluginId) const override;
    bool ReloadPluginConfig(const std::string &pluginId) override;

private:
    /**
     * @brief 插件条目
     */
    struct PluginEntry
    {
        std::shared_ptr<IPlugin> plugin;                     ///< 插件实例
        PluginInfo info;                                     ///< 插件信息
        std::shared_ptr<IDynamicLibraryLoader> loader;       ///< 库加载器
        std::map<std::string, std::any> config;              ///< 插件配置
    };

    /**
     * @brief 库条目
     */
    struct LibraryEntry
    {
        std::shared_ptr<IDynamicLibraryLoader> loader;           ///< 库加载器
        std::map<std::string, std::shared_ptr<IPlugin>> plugins; ///< 库中的插件
    };

    /**
     * @brief 内部加载插件
     * @param pluginPath 插件路径
     * @param entry 输出参数，插件条目
     * @return 成功返回true
     */
    bool LoadPluginInternal(const std::string &pluginPath, PluginEntry &entry);

    /**
     * @brief 内部卸载插件
     * @param pluginId 插件ID
     */
    void UnloadPluginInternal(const std::string &pluginId);

    /**
     * @brief 加载插件配置
     * @param pluginId 插件ID
     */
    void LoadPluginConfig(const std::string &pluginId);

    /**
     * @brief 保存插件配置
     * @param pluginId 插件ID
     */
    void SavePluginConfig(const std::string &pluginId);

    /**
     * @brief 通知插件事件
     * @param pluginId 插件ID
     * @param event 事件类型
     * @param data 事件数据
     */
    void NotifyPluginEvent(const std::string &pluginId, 
                           PluginEvent event, 
                           const std::any &data = std::any());

    /**
     * @brief 清理过期插件
     */
    void CleanupExpiredPlugins();

    mutable std::shared_mutex pluginsMutex_;                          ///< 插件互斥锁
    std::unordered_map<std::string, PluginEntry> plugins_;            ///< 插件映射表
    std::unordered_map<std::string, LibraryEntry> libraries_;         ///< 库映射表

    std::unordered_map<std::string, std::function<std::shared_ptr<IPlugin>()>> factories_; ///< 插件工厂

    PluginEventCallback eventCallback_;                               ///< 事件回调
    std::string configDir_;                                           ///< 配置目录

    std::atomic<bool> initialized_{false};                            ///< 初始化标志
    std::atomic<int> nextPluginId_{1};                                ///< 下一个插件ID
};

} // namespace PluginSystem

#endif // PLUGIN_SYSTEM_MANAGER_IMPL_H