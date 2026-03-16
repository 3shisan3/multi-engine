/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        PluginManager.h
Version:     1.0
Author:      cjx
start date: 2026-1-11
Description: 插件管理器接口
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-1-10      cjx            create
*****************************************************************/

#ifndef PLUGIN_SYSTEM_MANAGER_H
#define PLUGIN_SYSTEM_MANAGER_H

#include "PluginInterface.h"

#include <memory>
#include <string>
#include <vector>

namespace PluginSystem
{

/**
 * @brief 插件管理器接口
 */
class IPluginManager
{
public:
    virtual ~IPluginManager() = default;

    /**
     * @brief 初始化插件管理器
     * @param configDir 插件配置目录
     * @return 成功返回true
     */
    virtual bool Initialize(const std::string &configDir = "") = 0;

    /**
     * @brief 加载插件
     * @param pluginPath 插件库路径
     * @return 成功返回插件ID，失败返回空字符串
     */
    virtual std::string LoadPlugin(const std::string &pluginPath) = 0;

    /**
     * @brief 卸载插件
     * @param pluginId 插件ID
     * @return 成功返回true
     */
    virtual bool UnloadPlugin(const std::string &pluginId) = 0;

    /**
     * @brief 获取插件
     * @param pluginId 插件ID
     * @return 插件指针，失败返回nullptr
     */
    virtual std::shared_ptr<IPlugin> GetPlugin(const std::string &pluginId) const = 0;

    /**
     * @brief 获取所有插件信息
     */
    virtual std::vector<PluginInfo> GetAllPluginInfo() const = 0;

    /**
     * @brief 获取已加载的插件列表
     */
    virtual std::vector<std::string> GetLoadedPlugins() const = 0;

    /**
     * @brief 扫描插件目录
     * @param pluginDir 插件目录
     * @return 发现的插件数量
     */
    virtual int ScanPlugins(const std::string &pluginDir) = 0;

    /**
     * @brief 启动所有插件
     * @return 成功返回true
     */
    virtual bool StartAllPlugins() = 0;

    /**
     * @brief 停止所有插件
     */
    virtual void StopAllPlugins() = 0;

    /**
     * @brief 启动指定插件
     * @param pluginId 插件ID
     * @return 成功返回true
     */
    virtual bool StartPlugin(const std::string &pluginId) = 0;

    /**
     * @brief 停止指定插件
     * @param pluginId 插件ID
     */
    virtual void StopPlugin(const std::string &pluginId) = 0;

    /**
     * @brief 注册插件工厂
     * @param pluginType 插件类型
     * @param factory 工厂函数
     * @return 成功返回true
     */
    virtual bool RegisterPluginFactory(const std::string &pluginType,
                                       std::function<std::shared_ptr<IPlugin>()> factory) = 0;

    /**
     * @brief 设置插件事件回调
     * @param callback 回调函数
     */
    virtual void SetPluginEventCallback(PluginEventCallback callback) = 0;

    /**
     * @brief 获取插件管理器状态
     */
    virtual std::map<std::string, std::any> GetStatus() const = 0;

    /**
     * @brief 检查插件依赖是否满足
     */
    virtual bool CheckDependencies(const std::string &pluginId) const = 0;

    /**
     * @brief 重新加载插件配置
     */
    virtual bool ReloadPluginConfig(const std::string &pluginId) = 0;
};

/**
 * @brief 创建插件管理器实例
 */
PLUGIN_EXPORT std::shared_ptr<IPluginManager> CreatePluginManager();

} // namespace PluginSystem

#endif // PLUGIN_SYSTEM_MANAGER_H