/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        dds_adapter.h
Version:     1.0
Author:      cjx
start date:
Description: 针对dds协议的适配器实现，基于开源库CycloneDDS
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-1-04      cjx            create

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

    // DDS相关方法
#ifdef USE_DDS
    bool InitializeDDS();
    void CleanupDDS();

    bool CreateTopic(const std::string &topicName);
    bool DeleteTopic(const std::string &topicName);

    void ReaderThreadFunc();
    void ProcessIncomingMessages();
    void HandleDiscoveryEvent(dds_entity_t reader);

    bool SafePublish(const std::string &topic, const void *data, size_t size);
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