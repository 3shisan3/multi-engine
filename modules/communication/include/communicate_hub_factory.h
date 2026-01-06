/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        communicate_hub_factory.h
Version:     1.0
Author:      cjx
start date:
Description: 通信枢纽工厂接口
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-1-04      cjx            create

*****************************************************************/

#ifndef COMMUNICATION_HUB_FACTORY_H
#define COMMUNICATION_HUB_FACTORY_H

#include "CommunicationHub.h"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace CommunicationModule
{

/**
 * @brief 通信枢纽工厂
 * 负责创建通信枢纽和协议适配器实例
 */
class CommunicationHubFactory
{
public:
    CommunicationHubFactory() = delete;
    CommunicationHubFactory(const CommunicationHubFactory &) = delete;
    CommunicationHubFactory &operator=(const CommunicationHubFactory &) = delete;

    /**
     * @brief 创建通信枢纽实例
     */
    static std::shared_ptr<ICommunicationHub> CreateHub();

    /**
     * @brief 创建通信枢纽实例（自定义实现）
     */
    static std::shared_ptr<ICommunicationHub> CreateHub(std::function<std::shared_ptr<ICommunicationHub>()> factory);

    /**
     * @brief 创建协议适配器
     */
    static std::shared_ptr<IProtocolAdapter> CreateProtocolAdapter(const std::string &protocolType);

    /**
     * @brief 创建协议适配器（自定义实现）
     */
    static std::shared_ptr<IProtocolAdapter> CreateProtocolAdapter(
        const std::string &protocolType,
        std::function<std::shared_ptr<IProtocolAdapter>()> factory);

    /**
     * @brief 注册协议适配器工厂
     */
    static bool RegisterProtocolAdapter(const std::string &protocolType,
                                        std::function<std::shared_ptr<IProtocolAdapter>()> factory);

    /**
     * @brief 取消注册协议适配器工厂
     */
    static bool UnregisterProtocolAdapter(const std::string &protocolType);

    /**
     * @brief 获取支持的协议类型
     */
    static std::vector<std::string> GetSupportedProtocolTypes();

    /**
     * @brief 检查是否支持某个协议
     */
    static bool IsProtocolSupported(const std::string &protocolType);

    /**
     * @brief 设置默认协议适配器
     */
    static void SetDefaultProtocolAdapter(const std::string &protocolType);

    /**
     * @brief 获取默认协议适配器类型
     */
    static std::string GetDefaultProtocolAdapter();

private:
    // 协议适配器工厂函数映射
    static std::map<std::string, std::function<std::shared_ptr<IProtocolAdapter>()>> &GetFactoryMap();

    static std::string defaultProtocolAdapter_;
    static std::mutex factoryMutex_;
};

} // namespace CommunicationModule

#endif // COMMUNICATION_HUB_FACTORY_H