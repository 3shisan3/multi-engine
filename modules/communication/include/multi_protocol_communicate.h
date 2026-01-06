/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        multi_protocol_communicate.h
Version:     1.0
Author:      cjx
start date:
Description: 多协议通信枢纽实现
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-1-04      cjx            create

*****************************************************************/

#ifndef MULTI_PROTOCOL_COMMUNICATION_HUB_H
#define MULTI_PROTOCOL_COMMUNICATION_HUB_H

#include "CommunicationHub.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <unordered_set>


namespace CommunicationModule
{

/**
 * @brief 多协议通信枢纽实现
 * 统一管理多种通信协议，为上层提供统一接口
 */
class MultiProtocolCommunicationHub : public ICommunicationHub
{
public:
    MultiProtocolCommunicationHub();
    virtual ~MultiProtocolCommunicationHub();

    // 禁止拷贝和赋值
    MultiProtocolCommunicationHub(const MultiProtocolCommunicationHub &) = delete;
    MultiProtocolCommunicationHub &operator=(const MultiProtocolCommunicationHub &) = delete;

    // ICommunicationHub 接口实现
    bool Initialize(const std::vector<CommunicationConfig> &configs) override;
    bool Start() override;
    void Stop() override;
    void Shutdown() override;

    bool AddProtocolAdapter(const CommunicationConfig &config) override;
    bool RemoveProtocolAdapter(const std::string &protocolType) override;
    bool HasProtocolAdapter(const std::string &protocolType) const override;

    bool SendToClient(ClientId clientId, const NetworkMessage &message) override;
    int Broadcast(const NetworkMessage &message) override;
    int PublishToTopic(const std::string &topic, const NetworkMessage &message) override;
    bool SendViaProtocol(const std::string &protocolType,
                         ClientId clientId,
                         const NetworkMessage &message) override;

    bool RegisterDataReceiver(IDataReceiver *receiver) override;
    bool UnregisterDataReceiver(IDataReceiver *receiver) override;

    std::vector<ConnectionContext> GetConnectedClients() const override;
    std::shared_ptr<ConnectionContext> GetClientContext(ClientId clientId) const override;
    std::vector<std::shared_ptr<ConnectionContext>> FindClients(
        std::function<bool(const ConnectionContext &)> predicate) const override;

    bool DisconnectClient(ClientId clientId, const std::string &reason = "") override;

    bool SubscribeToTopic(const std::string &topic,
                          std::function<void(const ConnectionContext &,
                                             const NetworkMessage &)>
                              callback) override;
    bool UnsubscribeFromTopic(const std::string &topic) override;

    std::map<std::string, uint64_t> GetStatistics() const override;
    std::vector<std::string> GetSupportedProtocols() const override;
    std::map<std::string, bool> GetProtocolStatus() const override;

    void SetErrorCallback(std::function<void(const std::string &,
                                             const std::string &)>
                              callback) override;
    std::vector<std::string> GetRegisteredReceivers() const override;

    // 扩展功能
    /**
     * @brief 发送消息到指定客户端组
     */
    int SendToClientGroup(const std::vector<ClientId> &clientIds,
                          const NetworkMessage &message);

    /**
     * @brief 发送消息到指定类型的客户端
     */
    int SendToClientType(ClientType clientType, const NetworkMessage &message);

    /**
     * @brief 获取客户端订阅的主题
     */
    std::vector<std::string> GetClientSubscribedTopics(ClientId clientId) const;

    /**
     * @brief 检查客户端是否订阅了指定主题
     */
    bool IsClientSubscribedToTopic(ClientId clientId, const std::string &topic) const;

    /**
     * @brief 获取主题的订阅者列表
     */
    std::vector<ClientId> GetTopicSubscribers(const std::string &topic) const;

    /**
     * @brief 重置统计信息
     */
    void ResetStatistics();

private:
    // 内部数据结构
    struct ClientInfo
    {
        std::shared_ptr<ConnectionContext> context;
        std::string protocolType;
        int64_t lastActivityTime;
        std::unordered_set<std::string> subscribedTopics;
        std::mutex mutex;
    };

    // 使用手动定义的构造函数和移动操作来解决 atomic<bool> 的移动问题
    struct ProtocolAdapterInfo
    {
        std::shared_ptr<IProtocolAdapter> adapter;
        CommunicationConfig config;
        std::atomic<bool> isRunning{false};
        std::string name;
        
        // 默认构造函数
        ProtocolAdapterInfo() = default;
        
        // 自定义构造函数
        ProtocolAdapterInfo(std::shared_ptr<IProtocolAdapter> a, 
                           const CommunicationConfig& c, 
                           bool running, 
                           const std::string& n)
            : adapter(std::move(a))
            , config(c)
            , isRunning(running)
            , name(n) 
        {}
        
        // 删除拷贝构造函数和赋值操作符，避免 atomic 问题
        ProtocolAdapterInfo(const ProtocolAdapterInfo&) = delete;
        ProtocolAdapterInfo& operator=(const ProtocolAdapterInfo&) = delete;
        
        // 允许移动构造函数
        ProtocolAdapterInfo(ProtocolAdapterInfo&& other) noexcept
            : adapter(std::move(other.adapter))
            , config(std::move(other.config))
            , name(std::move(other.name))
        {
            isRunning.store(other.isRunning.load());
        }
        
        // 允许移动赋值操作符
        ProtocolAdapterInfo& operator=(ProtocolAdapterInfo&& other) noexcept
        {
            if (this != &other) {
                adapter = std::move(other.adapter);
                config = std::move(other.config);
                name = std::move(other.name);
                isRunning.store(other.isRunning.load());
            }
            return *this;
        }
    };

    struct TopicSubscription
    {
        std::string topic;
        std::vector<std::function<void(const ConnectionContext &, const NetworkMessage &)>> callbacks;
        std::mutex mutex;
    };

    // 消息分发器
    class MessageDispatcher
    {
    public:
        MessageDispatcher() = default;
        ~MessageDispatcher();

        bool AddReceiver(IDataReceiver *receiver);
        bool RemoveReceiver(IDataReceiver *receiver);
        void DispatchMessage(const ConnectionContext &context, const NetworkMessage &message);
        std::vector<IDataReceiver *> GetReceivers() const;
        std::vector<std::string> GetReceiverNames() const;

        void SetErrorCallback(std::function<void(const std::string &)> callback);

    private:
        mutable std::mutex receiversMutex_;
        std::unordered_map<std::string, IDataReceiver *> receivers_;
        std::function<void(const std::string &)> errorCallback_;
    };

    // 内部方法
    bool InitializeProtocolAdapter(const CommunicationConfig &config);
    void StartProtocolAdapter(const std::string &protocolType);
    void StopProtocolAdapter(const std::string &protocolType);

    void HandleIncomingMessage(const std::string &protocolType,
                               const ConnectionContext &context,
                               const NetworkMessage &message);
    void UpdateClientActivity(ClientId clientId);
    void CleanupInactiveClients();
    std::string SelectProtocolForClient(ClientId clientId) const;

    // 线程函数
    void MaintenanceThreadFunc();
    void UpdateStatistics(const std::string &key, uint64_t delta);

    // 协议适配器回调
    void OnProtocolMessageReceived(const std::string &protocolType,
                                   const ConnectionContext &context,
                                   const NetworkMessage &message);
    void OnProtocolConnectionChanged(const std::string &protocolType,
                                     const ConnectionContext &context,
                                     bool connected);
    void OnProtocolError(const std::string &protocolType,
                         const std::string &errorMessage,
                         int errorCode);

    // 成员变量
    mutable std::mutex adaptersMutex_;
    std::unordered_map<std::string, ProtocolAdapterInfo> protocolAdapters_;

    mutable std::mutex clientsMutex_;
    std::unordered_map<ClientId, std::shared_ptr<ClientInfo>> connectedClients_;
    std::unordered_map<std::string, std::unordered_set<ClientId>> protocolClients_;

    mutable std::mutex topicsMutex_;
    std::unordered_map<std::string, std::shared_ptr<TopicSubscription>> topicSubscriptions_;
    std::unordered_map<std::string, std::unordered_set<ClientId>> topicSubscribers_;

    MessageDispatcher messageDispatcher_;

    std::atomic<bool> running_{false};
    std::thread maintenanceThread_;
    std::condition_variable maintenanceCV_;

    // 统计信息
    mutable std::mutex statsMutex_;
    std::map<std::string, uint64_t> statistics_;

    // 配置
    int heartbeatIntervalMs_ = 1000;
    int connectionTimeoutMs_ = 5000;
    int maintenanceIntervalMs_ = 5000;

    std::string defaultProtocol_;

    // 错误回调
    std::function<void(const std::string &, const std::string &)> errorCallback_;

    // 序列号生成
    std::atomic<uint64_t> nextClientId_{1};
    std::atomic<uint64_t> nextMessageId_{1};
};

} // namespace CommunicationModule

#endif // MULTI_PROTOCOL_COMMUNICATION_HUB_H