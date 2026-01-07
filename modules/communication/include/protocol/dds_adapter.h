/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        dds_adapter.h
Version:     1.1
Author:      cjx
start date:
Description: 针对dds协议的适配器实现，基于开源库CycloneDDS（完整版）
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-1-04      cjx            create
2             2026-1-07      cjx            添加完整主题管理接口

*****************************************************************/

#ifndef DDS_PROTOCOL_ADAPTER_H
#define DDS_PROTOCOL_ADAPTER_H

#include "protocol/base_adapter.h"

#include <queue>
#include <unordered_map>
#include <unordered_set>

// CycloneDDS头文件
#ifdef USE_DDS
#include <dds/dds.h>
#endif

namespace CommunicationModule
{

/**
 * @brief DDS协议适配器
 * 使用CycloneDDS实现DDS协议支持
 */
class DDSProtocolAdapter : public BaseProtocolAdapter
{
public:
    DDSProtocolAdapter();
    virtual ~DDSProtocolAdapter();

    // IProtocolAdapter 接口实现
    bool SendToClient(ClientId clientId, const NetworkMessage &message) override;
    int Broadcast(const NetworkMessage &message) override;
    int PublishToTopic(const std::string &topic, const NetworkMessage &message) override;

    std::vector<ConnectionContext> GetConnectedClients() const override;
    bool DisconnectClient(ClientId clientId, const std::string &reason = "") override;

    size_t GetConnectionCount() const override;
    size_t GetActiveConnectionCount() const override;

    bool SendRawData(ClientId clientId, const std::vector<uint8_t> &data) override;

protected:
    bool DoInitialize() override;
    bool DoStart() override;
    void DoStop() override;
    void DoCleanup() override;

private:
    struct DDSTopicInfo
    {
        std::string topicName;
        dds_entity_t topic;
        dds_entity_t writer;
        dds_entity_t reader;
        std::unordered_set<ClientId> subscribers;
    };

    struct DDSClientInfo
    {
        ConnectionContext context;
        dds_instance_handle_t instanceHandle;
        int64_t lastSeenTime;
        std::unordered_set<std::string> subscribedTopics;
    };

    struct DDSTransportMessage
    {
        uint64_t messageId;
        uint64_t sourceClientId;
        uint64_t targetClientId;
        uint32_t messageType;
        uint64_t timestamp;
        uint32_t payloadSize;
        uint32_t headerSize;
        uint32_t checksum;
    };

#ifdef USE_DDS
    bool InitializeDDS();
    void CleanupDDS();

    bool CreateTopic(const std::string &topicName);

    /**
     * @brief 删除DDS主题
     * @param topicName 主题名称
     * @return 成功返回true，失败返回false
     */
    bool DeleteTopic(const std::string &topicName);

    /**
     * @brief 更新主题的QoS配置
     * @param topicName 主题名称
     * @param qosParams QoS参数映射
     * @return 成功返回true，失败返回false
     */
    bool UpdateTopicQoS(const std::string &topicName, 
                       const std::map<std::string, std::any>& qosParams);

    /**
     * @brief 获取所有主题列表
     * @return 主题名称列表
     */
    std::vector<std::string> GetAllTopics() const;

    /**
     * @brief 获取主题的订阅者列表
     * @param topicName 主题名称
     * @return 订阅该主题的客户端ID列表
     */
    std::vector<ClientId> GetTopicSubscribers(const std::string &topicName) const;

    /**
     * @brief 检查主题是否存在
     * @param topicName 主题名称
     * @return 存在返回true，否则返回false
     */
    bool IsTopicExists(const std::string &topicName) const;

    /**
     * @brief 重新扫描和清理主题及客户端
     * 清理无效的订阅者和不活跃的客户端
     */
    void RescanTopics();

    /**
     * @brief 订阅客户端到主题
     * @param clientId 客户端ID
     * @param topicName 主题名称
     * @return 成功返回true，失败返回false
     */
    bool SubscribeClientToTopic(ClientId clientId, const std::string &topicName);

    /**
     * @brief 取消客户端对主题的订阅
     * @param clientId 客户端ID
     * @param topicName 主题名称
     * @return 成功返回true，失败返回false
     */
    bool UnsubscribeClientFromTopic(ClientId clientId, const std::string &topicName);

    /**
     * @brief 获取客户端的订阅列表
     * @param clientId 客户端ID
     * @return 客户端订阅的主题名称列表
     */
    std::vector<std::string> GetClientSubscriptions(ClientId clientId) const;

    /**
     * @brief 获取主题统计信息
     * @param topicName 主题名称
     * @return 主题统计信息映射
     */
    std::map<std::string, std::any> GetTopicStats(const std::string &topicName) const;

    /**
     * @brief 批量创建主题
     * @param topicNames 主题名称列表
     * @return 成功创建的主题数量
     */
    int BatchCreateTopics(const std::vector<std::string> &topicNames);

    /**
     * @brief 批量删除主题
     * @param topicNames 主题名称列表
     * @return 成功删除的主题数量
     */
    int BatchDeleteTopics(const std::vector<std::string> &topicNames);
    
    void ReaderThreadFunc();
    void HandleDiscoveryEvent(dds_entity_t reader);

    bool SafePublish(const std::string &topic, const void *data, size_t size);
    
    /**
     * @brief 处理DDS发现错误
     * @param errorCode DDS错误码
     * @param operation 发生错误的操作名称
     */
    void HandleDDSDiscoveryError(int errorCode, const std::string &operation);
#endif

    // 序列化/反序列化
    std::vector<uint8_t> SerializeDDSMessage(const NetworkMessage &msg) const;
    NetworkMessage DeserializeDDSMessage(const std::vector<uint8_t> &data) const;

    // 辅助函数
    void LogDDSError(int errorCode, const std::string &operation);
    uint32_t CalculateChecksum(const std::vector<uint8_t> &data) const;

    // 线程函数
    void MaintenanceThreadFunc();

#ifdef USE_DDS
    // DDS实体
    dds_entity_t participant_;
    dds_entity_t publisher_;
    dds_entity_t subscriber_;
#endif

    // 主题管理
    mutable std::mutex topicsMutex_;
    std::unordered_map<std::string, std::shared_ptr<DDSTopicInfo>> topics_;

    // 客户端管理
    mutable std::mutex clientsMutex_;
    std::unordered_map<ClientId, DDSClientInfo> ddsClients_;
    std::unordered_map<uint64_t, ClientId> guidToClient_;

    // 线程
    std::atomic<bool> shouldStop_{false};
    std::thread readerThread_;
    std::thread maintenanceThread_;

    // 配置
    uint32_t domainId_ = 0;
    std::string participantName_;
    int qosHistoryDepth_ = 10;
    int qosReliabilityKind_ = 1; // DDS_RELIABILITY_RELIABLE

    // 统计 - 使用 mutable 允许在 const 函数中修改
    mutable std::atomic<uint64_t> messagesPublished_{0};
    mutable std::atomic<uint64_t> messagesReceived_{0};
    mutable std::atomic<uint64_t> topicsCreated_{0};
};

} // namespace CommunicationModule

#endif // DDS_PROTOCOL_ADAPTER_H