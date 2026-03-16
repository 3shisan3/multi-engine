/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        PluginInterface.h
Version:     1.0
Author:      cjx
start date: 2026-2-06
Description: 通用插件接口定义
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-2-06      cjx            create
*****************************************************************/

#ifndef PLUGIN_SYSTEM_INTERFACE_H
#define PLUGIN_SYSTEM_INTERFACE_H

#include <any>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace PluginSystem
{

// 插件基类的前置声明
class IPlugin;

/**
 * @brief 导出函数类型
 */
using CreatePluginFunc = IPlugin* (*)();
using DestroyPluginFunc = void (*)(IPlugin*);

/**
 * @brief 插件信息
 */
struct PluginInfo
{
    std::string pluginId;                        // 插件唯一标识
    std::string pluginName;                      // 插件名称
    std::string pluginVersion;                   // 插件版本
    std::string pluginType;                      // 插件类型
    std::string description;                     // 插件描述
    std::string author;                          // 作者
    std::string license;                         // 许可证
    std::vector<std::string> dependencies;       // 依赖的插件列表
    std::map<std::string, std::string> metadata; // 元数据
    std::string libraryPath;                     // 插件库路径
    bool isLoaded = false;                       // 是否已加载
    bool isEnabled = false;                      // 是否启用
};

/**
 * @brief 插件接口
 * 所有插件必须实现此接口
 */
class IPlugin
{
public:
    virtual ~IPlugin() = default;

    /**
     * @brief 获取插件信息
     */
    virtual PluginInfo GetPluginInfo() const = 0;

    /**
     * @brief 初始化插件
     * @param config 插件配置
     * @return 成功返回true
     */
    virtual bool Initialize(const std::map<std::string, std::any> &config) = 0;

    /**
     * @brief 启动插件
     * @return 成功返回true
     */
    virtual bool Start() = 0;

    /**
     * @brief 停止插件
     */
    virtual void Stop() = 0;

    /**
     * @brief 卸载插件
     */
    virtual void Unload() = 0;

    /**
     * @brief 获取插件状态
     * @return 状态字符串
     */
    virtual std::string GetStatus() const = 0;

    /**
     * @brief 处理命令
     * @param command 命令
     * @param params 参数
     * @return 处理结果
     */
    virtual std::any ExecuteCommand(const std::string &command,
                                    const std::map<std::string, std::any> &params) = 0;

    /**
     * @brief 获取插件类型
     * @return 插件类型字符串
     */
    virtual std::string GetPluginType() const = 0;
};

/**
 * @brief 插件事件类型
 */
enum class PluginEvent
{
    LOADED,   // 插件加载
    UNLOADED, // 插件卸载
    STARTED,  // 插件启动
    STOPPED,  // 插件停止
    ERROR     // 插件错误
};

/**
 * @brief 插件事件数据
 */
struct PluginEventData
{
    std::string pluginId;
    PluginEvent event;
    std::any data;
    int64_t timestamp;
};

/**
 * @brief 插件事件回调函数类型
 */
using PluginEventCallback = std::function<void(const PluginEventData &)>;

} // namespace PluginSystem

#endif // PLUGIN_SYSTEM_INTERFACE_H