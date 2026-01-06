/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        base_protocol_adapter.h
Version:     1.0
Author:      cjx
start date:
Description: 基础协议适配器接口
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-1-04      cjx            create

*****************************************************************/

#ifndef BASE_PROTOCOL_ADAPTER_H
#define BASE_PROTOCOL_ADAPTER_H

#include "CommunicationHub.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <functional>

namespace CommunicationModule
{

/**
 * @brief 基础协议适配器
 * 提供协议适配器的基本实现，具体协议可继承此类
 */
class BaseProtocolAdapter : public IProtocolAdapter
{
public:
    BaseProtocolAdapter(const std::string& protocolType, const std::string& adapterName);
    virtual ~BaseProtocolAdapter();
    
    // IProtocolAdapter 接口实现
    bool Initialize(const CommunicationConfig& config) override;
    virtual bool Start() override;
    virtual void Stop() override;
    
    virtual bool SendToClient(ClientId clientId, const NetworkMessage& message) override = 0;
    virtual int Broadcast(const NetworkMessage& message) override = 0;
    virtual int PublishToTopic(const std::string& topic, const NetworkMessage& message) override = 0;
    
    virtual std::vector<ConnectionContext> GetConnectedClients() const override = 0;
    virtual bool DisconnectClient(ClientId clientId, const std::string& reason = "") override = 0;
    
    std::string GetAdapterName() const override { return adapterName_; }
    std::string GetProtocolType() const override { return protocolType_; }
    
    virtual std::map<std::string, uint64_t> GetStatistics() const override;
    virtual void ResetStatistics() override;
    
    void SetMessageCallback(std::function<void(const ConnectionContext&, const NetworkMessage&)> callback) override;

    void SetConnectionCallback(std::function<void(const ConnectionContext&, bool connected)> callback) override;
    
    void SetErrorCallback(std::function<void(const std::string&, int)> callback) override;
    
    bool IsRunning() const override { return isRunning_; }
    
    // 扩展功能
    /**
     * @brief 发送原始数据
     */
    virtual bool SendRawData(ClientId clientId, const std::vector<uint8_t>& data) = 0;
    
    /**
     * @brief 获取连接数
     */
    virtual size_t GetConnectionCount() const = 0;
    
    /**
     * @brief 获取活跃连接数
     */
    virtual size_t GetActiveConnectionCount() const = 0;

protected:
    /**
     * @brief 具体的初始化实现（由子类实现）
     */
    virtual bool DoInitialize() = 0;
    
    /**
     * @brief 具体的启动实现（由子类实现）
     */
    virtual bool DoStart() = 0;
    
    /**
     * @brief 具体的停止实现（由子类实现）
     */
    virtual void DoStop() = 0;
    
    /**
     * @brief 具体的清理实现（由子类实现）
     */
    virtual void DoCleanup() = 0;
    
    /**
     * @brief 通知消息到达
     */
    void NotifyMessageReceived(const ConnectionContext& context, const NetworkMessage& message);
    
    /**
     * @brief 通知连接状态变化
     */
    void NotifyConnectionChanged(const ConnectionContext& context, bool connected);
    
    /**
     * @brief 通知错误发生
     */
    void NotifyError(const std::string& errorMessage, int errorCode = 0);
    
    /**
     * @brief 更新统计信息
     */
    void UpdateStatistic(const std::string& key, uint64_t delta);
    
    /**
     * @brief 获取配置
     */
    const CommunicationConfig& GetConfig() const { return config_; }
    
    /**
     * @brief 获取下一个客户端ID
     */
    ClientId GetNextClientId();
    
    /**
     * @brief 创建连接上下文
     */
    virtual ConnectionContext CreateConnectionContext(ClientId clientId, 
                                                     const std::string& remoteAddress,
                                                     int remotePort) const;

    // 序列化/反序列化接口
    virtual std::vector<uint8_t> SerializeMessage(const NetworkMessage& message) const;
    virtual NetworkMessage DeserializeMessage(const std::vector<uint8_t>& data) const;
    
    // 消息头处理
    virtual void AddMessageHeaders(NetworkMessage& message, 
                                  const std::map<std::string, std::string>& additionalHeaders = {}) const;
    virtual std::map<std::string, std::string> ParseMessageHeaders(const std::vector<uint8_t>& data) const;

private:
    std::string protocolType_;
    std::string adapterName_;
    CommunicationConfig config_;
    
    std::atomic<bool> isRunning_{false};
    std::atomic<bool> isInitialized_{false};
    
    std::function<void(const ConnectionContext&, const NetworkMessage&)> messageCallback_;
    std::function<void(const ConnectionContext&, bool connected)> connectionCallback_;
    std::function<void(const std::string&, int)> errorCallback_;
    
    mutable std::mutex statsMutex_;
    std::map<std::string, uint64_t> statistics_;
    
    std::atomic<ClientId> nextClientId_{1};
};

} // namespace CommunicationModule

#endif // BASE_PROTOCOL_ADAPTER_H