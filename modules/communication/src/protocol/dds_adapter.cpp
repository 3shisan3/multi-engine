#include "protocol/dds_adapter.h"

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace std::chrono;

namespace CommunicationModule
{

DDSProtocolAdapter::DDSProtocolAdapter()
    : BaseProtocolAdapter("DDS", "DDSProtocolAdapter")
#ifdef USE_DDS
    , participant_(0)
    , publisher_(0)
    , subscriber_(0)
#endif
{
}

DDSProtocolAdapter::~DDSProtocolAdapter()
{
    DoStop();
    DoCleanup();
}

bool DDSProtocolAdapter::DoInitialize()
{
#ifdef USE_DDS
    // 解析DDS配置参数
    auto domainIdIt = GetConfig().protocolParams.find("domain_id");
    if (domainIdIt != GetConfig().protocolParams.end())
    {
        domainId_ = std::stoi(domainIdIt->second);
    }

    auto participantNameIt = GetConfig().protocolParams.find("participant_name");
    if (participantNameIt != GetConfig().protocolParams.end())
    {
        participantName_ = participantNameIt->second;
    }
    else
    {
        participantName_ = "GenericDDSAdapter";
    }

    auto qosHistoryIt = GetConfig().protocolParams.find("qos_history_depth");
    if (qosHistoryIt != GetConfig().protocolParams.end())
    {
        qosHistoryDepth_ = std::stoi(qosHistoryIt->second);
    }

    auto qosReliabilityIt = GetConfig().protocolParams.find("qos_reliability");
    if (qosReliabilityIt != GetConfig().protocolParams.end())
    {
        qosReliabilityKind_ = std::stoi(qosReliabilityIt->second);
    }

    return InitializeDDS();
#else
    NotifyError("DDS support not compiled in", -1);
    return false;
#endif
}

bool DDSProtocolAdapter::DoStart()
{
#ifdef USE_DDS
    if (shouldStop_)
    {
        return false;
    }

    // 启动读取线程
    readerThread_ = std::thread(&DDSProtocolAdapter::ReaderThreadFunc, this);

    // 启动维护线程
    maintenanceThread_ = std::thread(&DDSProtocolAdapter::MaintenanceThreadFunc, this);

    return true;
#else
    return false;
#endif
}

void DDSProtocolAdapter::DoStop()
{
    shouldStop_ = true;

    if (readerThread_.joinable())
    {
        readerThread_.join();
    }

    if (maintenanceThread_.joinable())
    {
        maintenanceThread_.join();
    }

#ifdef USE_DDS
    CleanupDDS();
#endif
}

void DDSProtocolAdapter::DoCleanup()
{
#ifdef USE_DDS
    // DDS清理在Stop中完成
#endif
}

bool DDSProtocolAdapter::SendToClient(ClientId clientId, const NetworkMessage &message)
{
#ifdef USE_DDS
    if (!IsRunning())
    {
        return false;
    }

    // 查找客户端订阅的主题
    std::string targetTopic;
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        auto clientIt = ddsClients_.find(clientId);
        if (clientIt == ddsClients_.end())
        {
            return false;
        }

        if (!message.topic.empty())
        {
            targetTopic = message.topic;
        }
        else if (!clientIt->second.subscribedTopics.empty())
        {
            targetTopic = *clientIt->second.subscribedTopics.begin();
        }
        else
        {
            return false;
        }
    }

    // 确保主题存在
    {
        std::lock_guard<std::mutex> lock(topicsMutex_);
        if (topics_.find(targetTopic) == topics_.end())
        {
            if (!CreateTopic(targetTopic))
            {
                return false;
            }
        }
    }

    // 序列化并发布消息
    auto data = SerializeDDSMessage(message);

    {
        std::lock_guard<std::mutex> lock(topicsMutex_);
        auto topicIt = topics_.find(targetTopic);
        if (topicIt != topics_.end())
        {
            if (SafePublish(targetTopic, data.data(), data.size()))
            {
                messagesPublished_.fetch_add(1);
                UpdateStatistic("messages_sent", 1);
                UpdateStatistic("bytes_sent", data.size());
                return true;
            }
        }
    }
#endif

    return false;
}

int DDSProtocolAdapter::Broadcast(const NetworkMessage &message)
{
#ifdef USE_DDS
    if (!IsRunning())
    {
        return 0;
    }

    std::vector<std::string> activeTopics;
    {
        std::lock_guard<std::mutex> lock(topicsMutex_);
        for (const auto &pair : topics_)
        {
            if (!pair.second->subscribers.empty())
            {
                activeTopics.push_back(pair.first);
            }
        }
    }

    auto data = SerializeDDSMessage(message);
    int successCount = 0;

    for (const auto &topic : activeTopics)
    {
        if (SafePublish(topic, data.data(), data.size()))
        {
            int subscriberCount = static_cast<int>(topics_[topic]->subscribers.size());
            successCount += subscriberCount;
        }
    }

    if (successCount > 0)
    {
        messagesPublished_.fetch_add(successCount);
        UpdateStatistic("messages_sent", successCount);
        UpdateStatistic("bytes_sent", data.size() * successCount);
    }

    return successCount;
#else
    return 0;
#endif
}

int DDSProtocolAdapter::PublishToTopic(const std::string &topic, const NetworkMessage &message)
{
#ifdef USE_DDS
    if (!IsRunning() || topic.empty())
    {
        return 0;
    }

    // 确保主题存在
    {
        std::lock_guard<std::mutex> lock(topicsMutex_);
        if (topics_.find(topic) == topics_.end())
        {
            if (!CreateTopic(topic))
            {
                return 0;
            }
        }
    }

    // 序列化并发布
    auto data = SerializeDDSMessage(message);

    if (SafePublish(topic, data.data(), data.size()))
    {
        std::lock_guard<std::mutex> lock(topicsMutex_);
        auto topicIt = topics_.find(topic);
        if (topicIt != topics_.end())
        {
            int subscriberCount = static_cast<int>(topicIt->second->subscribers.size());
            messagesPublished_.fetch_add(1);
            UpdateStatistic("messages_sent", subscriberCount);
            UpdateStatistic("bytes_sent", data.size() * subscriberCount);
            return subscriberCount;
        }
    }
#endif

    return 0;
}

std::vector<ConnectionContext> DDSProtocolAdapter::GetConnectedClients() const
{
    std::vector<ConnectionContext> clients;

#ifdef USE_DDS
    std::lock_guard<std::mutex> lock(clientsMutex_);
    clients.reserve(ddsClients_.size());

    for (const auto &pair : ddsClients_)
    {
        clients.push_back(pair.second.context);
    }
#endif

    return clients;
}

bool DDSProtocolAdapter::DisconnectClient(ClientId clientId, const std::string &reason)
{
#ifdef USE_DDS
    std::lock_guard<std::mutex> lock(clientsMutex_);

    auto it = ddsClients_.find(clientId);
    if (it == ddsClients_.end())
    {
        return false;
    }

    // 通知连接断开
    NotifyConnectionChanged(it->second.context, false);

    // 从主题订阅者中移除
    {
        std::lock_guard<std::mutex> topicLock(topicsMutex_);
        for (auto &topicPair : topics_)
        {
            topicPair.second->subscribers.erase(clientId);
        }
    }

    // 移除客户端
    ddsClients_.erase(it);

    UpdateStatistic("disconnections", 1);
    return true;
#else
    return false;
#endif
}

size_t DDSProtocolAdapter::GetConnectionCount() const
{
#ifdef USE_DDS
    std::lock_guard<std::mutex> lock(clientsMutex_);
    return ddsClients_.size();
#else
    return 0;
#endif
}

size_t DDSProtocolAdapter::GetActiveConnectionCount() const
{
    // DDS中所有连接都被视为活跃
    return GetConnectionCount();
}

bool DDSProtocolAdapter::SendRawData(ClientId clientId, const std::vector<uint8_t> &data)
{
    // DDS协议需要特定的消息格式
    NetworkMessage message;
    message.payload = data;
    message.type = MessageType::CUSTOM;
    return SendToClient(clientId, message);
}

// =============== DDS内部方法实现 ===============

#ifdef USE_DDS
bool DDSProtocolAdapter::DeleteTopic(const std::string &topicName)
{
    std::lock_guard<std::mutex> lock(topicsMutex_);
    
    auto it = topics_.find(topicName);
    if (it == topics_.end())
    {
        return false; // 主题不存在
    }
    
    // 清理主题的所有订阅者
    auto& topicInfo = it->second;
    
    // 从所有客户端的订阅列表中移除该主题
    {
        std::lock_guard<std::mutex> clientLock(clientsMutex_);
        for (auto& clientPair : ddsClients_)
        {
            clientPair.second.subscribedTopics.erase(topicName);
        }
    }
    
    // 删除DDS实体
    if (topicInfo->reader)
    {
        dds_delete(topicInfo->reader);
    }
    if (topicInfo->writer)
    {
        dds_delete(topicInfo->writer);
    }
    if (topicInfo->topic)
    {
        dds_delete(topicInfo->topic);
    }
    
    // 从主题列表中移除
    topics_.erase(it);
    
    // 更新统计
    UpdateStatistic("topics_deleted", 1);
    
    return true;
}

bool DDSProtocolAdapter::UpdateTopicQoS(const std::string &topicName, 
                                       const std::map<std::string, std::any>& qosParams)
{
    std::lock_guard<std::mutex> lock(topicsMutex_);
    
    auto it = topics_.find(topicName);
    if (it == topics_.end())
    {
        return false; // 主题不存在
    }
    
    // 解析QoS参数
    try
    {
        auto& topicInfo = it->second;
        
        // 获取新的QoS配置
        dds_qos_t *newQos = dds_create_qos();
        
        // 设置可靠性
        auto reliabilityIt = qosParams.find("reliability");
        if (reliabilityIt != qosParams.end())
        {
            std::string reliability = std::any_cast<std::string>(reliabilityIt->second);
            if (reliability == "reliable")
            {
                dds_qset_reliability(newQos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
            }
            else if (reliability == "best_effort")
            {
                dds_qset_reliability(newQos, DDS_RELIABILITY_BEST_EFFORT, DDS_SECS(0));
            }
        }
        
        // 设置历史深度
        auto historyIt = qosParams.find("history_depth");
        if (historyIt != qosParams.end())
        {
            int depth = std::any_cast<int>(historyIt->second);
            dds_qset_history(newQos, DDS_HISTORY_KEEP_LAST, depth);
        }
        
        // 设置持久性
        auto durabilityIt = qosParams.find("durability");
        if (durabilityIt != qosParams.end())
        {
            std::string durability = std::any_cast<std::string>(durabilityIt->second);
            if (durability == "transient")
            {
                dds_qset_durability(newQos, DDS_DURABILITY_TRANSIENT);
            }
            else if (durability == "persistent")
            {
                dds_qset_durability(newQos, DDS_DURABILITY_PERSISTENT);
            }
            else
            {
                dds_qset_durability(newQos, DDS_DURABILITY_VOLATILE);
            }
        }
        
        // 应用新的QoS到DataWriter和DataReader
        if (topicInfo->writer)
        {
            dds_set_qos(topicInfo->writer, newQos);
        }
        
        if (topicInfo->reader)
        {
            dds_set_qos(topicInfo->reader, newQos);
        }
        
        dds_delete_qos(newQos);
        
        return true;
    }
    catch (const std::exception& e)
    {
        NotifyError(std::string("Failed to update topic QoS: ") + e.what(), -10);
        return false;
    }
}

std::vector<std::string> DDSProtocolAdapter::GetAllTopics() const
{
    std::lock_guard<std::mutex> lock(topicsMutex_);
    
    std::vector<std::string> topicList;
    topicList.reserve(topics_.size());
    
    for (const auto& pair : topics_)
    {
        topicList.push_back(pair.first);
    }
    
    return topicList;
}

std::vector<ClientId> DDSProtocolAdapter::GetTopicSubscribers(const std::string &topicName) const
{
    std::lock_guard<std::mutex> lock(topicsMutex_);
    
    auto it = topics_.find(topicName);
    if (it == topics_.end())
    {
        return {};
    }
    
    const auto& subscribers = it->second->subscribers;
    return std::vector<ClientId>(subscribers.begin(), subscribers.end());
}

bool DDSProtocolAdapter::IsTopicExists(const std::string &topicName) const
{
    std::lock_guard<std::mutex> lock(topicsMutex_);
    return topics_.find(topicName) != topics_.end();
}

void DDSProtocolAdapter::RescanTopics()
{
    // 重新扫描所有主题，清理无效的订阅者
    std::lock_guard<std::mutex> topicsLock(topicsMutex_);
    std::lock_guard<std::mutex> clientsLock(clientsMutex_);
    
    auto now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    
    for (auto& topicPair : topics_)
    {
        auto& topicInfo = topicPair.second;
        auto& subscribers = topicInfo->subscribers;
        
        // 移除不存在的客户端
        std::unordered_set<ClientId> validSubscribers;
        for (ClientId clientId : subscribers)
        {
            if (ddsClients_.find(clientId) != ddsClients_.end())
            {
                // 检查客户端是否仍然订阅该主题
                const auto& clientInfo = ddsClients_[clientId];
                if (clientInfo.subscribedTopics.find(topicPair.first) != 
                    clientInfo.subscribedTopics.end())
                {
                    validSubscribers.insert(clientId);
                }
            }
        }
        
        // 更新订阅者列表
        topicInfo->subscribers = std::move(validSubscribers);
    }
    
    // 清理不活跃的客户端
    std::vector<ClientId> clientsToRemove;
    for (const auto& clientPair : ddsClients_)
    {
        if (now - clientPair.second.lastSeenTime > GetConfig().connectionTimeoutMs)
        {
            clientsToRemove.push_back(clientPair.first);
        }
    }
    
    for (ClientId clientId : clientsToRemove)
    {
        // 从所有主题的订阅者中移除
        for (auto& topicPair : topics_)
        {
            topicPair.second->subscribers.erase(clientId);
        }
        
        // 从客户端列表中移除
        ddsClients_.erase(clientId);
        
        // 从GUID映射中移除
        for (auto it = guidToClient_.begin(); it != guidToClient_.end();)
        {
            if (it->second == clientId)
            {
                it = guidToClient_.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}

bool DDSProtocolAdapter::SubscribeClientToTopic(ClientId clientId, const std::string &topicName)
{
    std::lock_guard<std::mutex> clientsLock(clientsMutex_);
    std::lock_guard<std::mutex> topicsLock(topicsMutex_);
    
    // 检查客户端是否存在
    auto clientIt = ddsClients_.find(clientId);
    if (clientIt == ddsClients_.end())
    {
        return false;
    }
    
    // 检查主题是否存在
    auto topicIt = topics_.find(topicName);
    if (topicIt == topics_.end())
    {
        // 主题不存在，尝试创建
        if (!CreateTopic(topicName))
        {
            return false;
        }
        topicIt = topics_.find(topicName);
    }
    
    // 添加到客户端的订阅列表
    clientIt->second.subscribedTopics.insert(topicName);
    
    // 添加到主题的订阅者列表
    topicIt->second->subscribers.insert(clientId);
    
    return true;
}

bool DDSProtocolAdapter::UnsubscribeClientFromTopic(ClientId clientId, const std::string &topicName)
{
    std::lock_guard<std::mutex> clientsLock(clientsMutex_);
    std::lock_guard<std::mutex> topicsLock(topicsMutex_);
    
    // 从客户端的订阅列表中移除
    auto clientIt = ddsClients_.find(clientId);
    if (clientIt != ddsClients_.end())
    {
        clientIt->second.subscribedTopics.erase(topicName);
    }
    
    // 从主题的订阅者列表中移除
    auto topicIt = topics_.find(topicName);
    if (topicIt != topics_.end())
    {
        topicIt->second->subscribers.erase(clientId);
    }
    
    return true;
}

std::vector<std::string> DDSProtocolAdapter::GetClientSubscriptions(ClientId clientId) const
{
    std::lock_guard<std::mutex> lock(clientsMutex_);
    
    auto it = ddsClients_.find(clientId);
    if (it == ddsClients_.end())
    {
        return {};
    }
    
    const auto& subscribedTopics = it->second.subscribedTopics;
    return std::vector<std::string>(subscribedTopics.begin(), subscribedTopics.end());
}

std::map<std::string, std::any> DDSProtocolAdapter::GetTopicStats(const std::string &topicName) const
{
    std::map<std::string, std::any> stats;
    
#ifdef USE_DDS
    std::lock_guard<std::mutex> lock(topicsMutex_);
    
    auto it = topics_.find(topicName);
    if (it == topics_.end())
    {
        return stats;
    }
    
    const auto& topicInfo = it->second;
    
    // 获取DataWriter状态
    if (topicInfo->writer)
    {
        uint32_t writerStatus = 0;
        dds_return_t rc = dds_get_status_changes(topicInfo->writer, &writerStatus);
        if (rc == DDS_RETCODE_OK)
        {
            stats["writer_status"] = static_cast<int>(writerStatus);
        }
    }
    
    // 获取DataReader状态
    if (topicInfo->reader)
    {
        uint32_t readerStatus = 0;
        dds_return_t rc = dds_get_status_changes(topicInfo->reader, &readerStatus);
        if (rc == DDS_RETCODE_OK)
        {
            stats["reader_status"] = static_cast<int>(readerStatus);
        }
    }
    
    // 主题基本信息
    stats["topic_name"] = topicName;
    stats["subscriber_count"] = static_cast<int>(topicInfo->subscribers.size());
    stats["created_time"] = topicsCreated_.load();
#endif
    
    return stats;
}

int DDSProtocolAdapter::BatchCreateTopics(const std::vector<std::string> &topicNames)
{
    int successCount = 0;
    
    for (const auto& topicName : topicNames)
    {
        if (CreateTopic(topicName))
        {
            successCount++;
        }
    }
    
    return successCount;
}

int DDSProtocolAdapter::BatchDeleteTopics(const std::vector<std::string> &topicNames)
{
    int successCount = 0;
    
    for (const auto& topicName : topicNames)
    {
        if (DeleteTopic(topicName))
        {
            successCount++;
        }
    }
    
    return successCount;
}

bool DDSProtocolAdapter::InitializeDDS()
{
    // 创建DomainParticipant
    dds_qos_t *participantQos = dds_create_qos();
    dds_qset_reliability(participantQos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
    dds_qset_history(participantQos, DDS_HISTORY_KEEP_ALL, qosHistoryDepth_);

    participant_ = dds_create_participant(domainId_, participantQos, NULL);
    dds_delete_qos(participantQos);

    if (participant_ < 0)
    {
        LogDDSError(participant_, "dds_create_participant");
        return false;
    }

    // 创建Publisher
    dds_qos_t *publisherQos = dds_create_qos();
    publisher_ = dds_create_publisher(participant_, publisherQos, NULL);
    dds_delete_qos(publisherQos);

    if (publisher_ < 0)
    {
        LogDDSError(publisher_, "dds_create_publisher");
        CleanupDDS();
        return false;
    }

    // 创建Subscriber
    dds_qos_t *subscriberQos = dds_create_qos();
    subscriber_ = dds_create_subscriber(participant_, subscriberQos, NULL);
    dds_delete_qos(subscriberQos);

    if (subscriber_ < 0)
    {
        LogDDSError(subscriber_, "dds_create_subscriber");
        CleanupDDS();
        return false;
    }

    return true;
}

void DDSProtocolAdapter::CleanupDDS()
{
    // 清理主题
    {
        std::lock_guard<std::mutex> lock(topicsMutex_);
        for (auto &pair : topics_)
        {
            if (pair.second->reader)
                dds_delete(pair.second->reader);
            if (pair.second->writer)
                dds_delete(pair.second->writer);
            if (pair.second->topic)
                dds_delete(pair.second->topic);
        }
        topics_.clear();
    }

    // 清理DDS实体
    if (subscriber_)
        dds_delete(subscriber_);
    if (publisher_)
        dds_delete(publisher_);
    if (participant_)
        dds_delete(participant_);

    subscriber_ = 0;
    publisher_ = 0;
    participant_ = 0;
}

bool DDSProtocolAdapter::CreateTopic(const std::string &topicName)
{
    // 简化的主题创建
    dds_qos_t *topicQos = dds_create_qos();
    dds_qset_reliability(topicQos,
                         qosReliabilityKind_ == 1 ? DDS_RELIABILITY_RELIABLE : DDS_RELIABILITY_BEST_EFFORT,
                         DDS_SECS(10));
    dds_qset_history(topicQos, DDS_HISTORY_KEEP_LAST, qosHistoryDepth_);
    dds_qset_durability(topicQos, DDS_DURABILITY_VOLATILE);

    dds_entity_t topic = dds_create_topic(participant_,
                                          NULL, // 使用空描述符
                                          topicName.c_str(),
                                          topicQos,
                                          NULL);
    dds_delete_qos(topicQos);

    if (topic < 0)
    {
        LogDDSError(topic, "dds_create_topic");
        return false;
    }

    // 创建DataWriter
    dds_qos_t *writerQos = dds_create_qos();
    dds_qset_reliability(writerQos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
    dds_entity_t writer = dds_create_writer(publisher_, topic, writerQos, NULL);
    dds_delete_qos(writerQos);

    if (writer < 0)
    {
        LogDDSError(writer, "dds_create_writer");
        dds_delete(topic);
        return false;
    }

    // 创建DataReader
    dds_qos_t *readerQos = dds_create_qos();
    dds_qset_reliability(readerQos, DDS_RELIABILITY_RELIABLE, DDS_SECS(10));
    dds_qset_history(readerQos, DDS_HISTORY_KEEP_LAST, qosHistoryDepth_);
    dds_entity_t reader = dds_create_reader(subscriber_, topic, readerQos, NULL);
    dds_delete_qos(readerQos);

    if (reader < 0)
    {
        LogDDSError(reader, "dds_create_reader");
        dds_delete(writer);
        dds_delete(topic);
        return false;
    }

    // 保存Topic信息
    auto topicInfo = std::make_shared<DDSTopicInfo>();
    topicInfo->topicName = topicName;
    topicInfo->topic = topic;
    topicInfo->writer = writer;
    topicInfo->reader = reader;

    topics_[topicName] = topicInfo;
    topicsCreated_.fetch_add(1);

    UpdateStatistic("topics_created", 1);

    return true;
}

bool DDSProtocolAdapter::SafePublish(const std::string &topic, const void *data, size_t size)
{
    std::lock_guard<std::mutex> lock(topicsMutex_);

    auto it = topics_.find(topic);
    if (it == topics_.end())
    {
        return false;
    }

    dds_return_t rc = dds_write(it->second->writer, data);
    if (rc != DDS_RETCODE_OK)
    {
        LogDDSError(rc, "dds_write");
        return false;
    }

    return true;
}

void DDSProtocolAdapter::ReaderThreadFunc()
{
    const int MAX_SAMPLES = 10;

    while (!shouldStop_)
    {
        std::vector<dds_entity_t> readers;
        {
            std::lock_guard<std::mutex> lock(topicsMutex_);
            for (const auto &pair : topics_)
            {
                readers.push_back(pair.second->reader);
            }
        }

        for (dds_entity_t reader : readers)
        {
            void *samples[MAX_SAMPLES];
            dds_sample_info_t infos[MAX_SAMPLES];

            dds_return_t rc = dds_take(reader, samples, infos, MAX_SAMPLES, MAX_SAMPLES);

            if (rc > 0)
            {
                for (int i = 0; i < rc; i++)
                {
                    if (infos[i].valid_data)
                    {
                        // 处理数据
                        const char *charData = static_cast<const char *>(samples[i]);
                        size_t dataSize = static_cast<size_t>(infos[i].source_timestamp);

                        if (dataSize > 0 && charData != nullptr)
                        {
                            std::vector<uint8_t> rawData(charData, charData + dataSize);
                            NetworkMessage msg = DeserializeDDSMessage(rawData);

                            // 更新客户端活动
                            {
                                std::lock_guard<std::mutex> clientLock(clientsMutex_);
                                auto it = ddsClients_.find(msg.sourceClientId);
                                if (it != ddsClients_.end())
                                {
                                    it->second.lastSeenTime = duration_cast<milliseconds>(
                                                                  system_clock::now().time_since_epoch())
                                                                  .count();

                                    // 通知消息到达
                                    NotifyMessageReceived(it->second.context, msg);
                                }
                            }

                            messagesReceived_.fetch_add(1);
                            UpdateStatistic("messages_received", 1);
                            UpdateStatistic("bytes_received", dataSize);
                        }
                    }

                    // 处理发现事件
                    if (infos[i].instance_state & DDS_IST_ALIVE)
                    {
                        HandleDiscoveryEvent(reader);
                    }
                }

                dds_return_loan(reader, samples, rc);
            }
        }

        dds_sleepfor(DDS_MSECS(10));
    }
}

void DDSProtocolAdapter::HandleDiscoveryEvent(dds_entity_t reader)
{
    dds_instance_handle_t instances[100];
    size_t count = 100;

    dds_return_t rc = dds_get_matched_publications(reader, instances, count);

    if (rc >= 0)
    {
        count = static_cast<size_t>(rc);

        for (size_t i = 0; i < count; i++)
        {
            uint64_t guidHash = static_cast<uint64_t>(instances[i]);

            std::lock_guard<std::mutex> lock(clientsMutex_);
            auto it = guidToClient_.find(guidHash);
            if (it == guidToClient_.end())
            {
                // 新客户端
                ClientId newClientId = GetNextClientId();

                DDSClientInfo clientInfo;
                clientInfo.context = CreateConnectionContext(newClientId, "dds://", 0);
                clientInfo.instanceHandle = instances[i];
                clientInfo.lastSeenTime = duration_cast<milliseconds>(
                                              system_clock::now().time_since_epoch())
                                              .count();

                ddsClients_[newClientId] = clientInfo;
                guidToClient_[guidHash] = newClientId;

                // 通知新连接
                NotifyConnectionChanged(clientInfo.context, true);

                UpdateStatistic("connections", 1);
            }
        }
    }
}

void DDSProtocolAdapter::HandleDDSDiscoveryError(int errorCode, const std::string &operation)
{
    const char *errorMsg = dds_strretcode(-errorCode);
    std::string fullError = "DDS discovery error in " + operation + ": " + 
                           errorMsg + " (code: " + std::to_string(errorCode) + ")";
    
    NotifyError(fullError, errorCode);
    
    // 根据错误类型采取不同的恢复措施
    if (errorCode == DDS_RETCODE_OUT_OF_RESOURCES)
    {
        // 清理一些资源
        RescanTopics();
    }
    else if (errorCode == DDS_RETCODE_TIMEOUT)
    {
        // 可以在这里实现重试逻辑
        // 例如：延迟后重新尝试发现
    }
}
#endif // USE_DDS

std::vector<uint8_t> DDSProtocolAdapter::SerializeDDSMessage(const NetworkMessage &msg) const
{
    // 计算总大小
    size_t totalSize = sizeof(DDSTransportMessage);

    // 计算headers大小
    size_t headersSize = 0;
    for (const auto &header : msg.headers)
    {
        headersSize += header.first.size() + 1;
        headersSize += header.second.size() + 1;
    }

    totalSize += headersSize + msg.payload.size();

    // 分配缓冲区
    std::vector<uint8_t> buffer(totalSize);
    uint8_t *ptr = buffer.data();

    // 填充消息头
    DDSTransportMessage *ddsMsg = reinterpret_cast<DDSTransportMessage *>(ptr);

    // 使用 fetch_add 获取并递增消息ID
    uint64_t messageId = messagesPublished_.fetch_add(1) + 1;
    ddsMsg->messageId = messageId;

    ddsMsg->sourceClientId = msg.sourceClientId;
    ddsMsg->targetClientId = msg.targetClientId;
    ddsMsg->messageType = static_cast<uint32_t>(msg.type);
    ddsMsg->timestamp = msg.timestamp;
    ddsMsg->payloadSize = static_cast<uint32_t>(msg.payload.size());
    ddsMsg->headerSize = static_cast<uint32_t>(headersSize);

    ptr += sizeof(DDSTransportMessage);

    // 序列化headers
    for (const auto &header : msg.headers)
    {
        std::strcpy(reinterpret_cast<char *>(ptr), header.first.c_str());
        ptr += header.first.size() + 1;

        std::strcpy(reinterpret_cast<char *>(ptr), header.second.c_str());
        ptr += header.second.size() + 1;
    }

    // 复制payload
    if (!msg.payload.empty())
    {
        std::memcpy(ptr, msg.payload.data(), msg.payload.size());
    }

    // 计算校验和
    ddsMsg->checksum = CalculateChecksum(buffer);

    return buffer;
}

NetworkMessage DDSProtocolAdapter::DeserializeDDSMessage(const std::vector<uint8_t> &data) const
{
    NetworkMessage msg;

    if (data.size() < sizeof(DDSTransportMessage))
    {
        return msg;
    }

    const DDSTransportMessage *ddsMsg = reinterpret_cast<const DDSTransportMessage *>(data.data());
    const uint8_t *ptr = data.data() + sizeof(DDSTransportMessage);

    // 基本字段
    msg.sourceClientId = ddsMsg->sourceClientId;
    msg.targetClientId = ddsMsg->targetClientId;
    msg.type = static_cast<MessageType>(ddsMsg->messageType);
    msg.timestamp = ddsMsg->timestamp;

    // 解析headers
    for (uint32_t i = 0; i < ddsMsg->headerSize;)
    {
        std::string key(reinterpret_cast<const char *>(ptr));
        ptr += key.size() + 1;
        i += static_cast<uint32_t>(key.size() + 1);

        std::string value(reinterpret_cast<const char *>(ptr));
        ptr += value.size() + 1;
        i += static_cast<uint32_t>(value.size() + 1);

        msg.headers[key] = value;
    }

    // 提取payload
    if (ddsMsg->payloadSize > 0 &&
        (ptr + ddsMsg->payloadSize) <= (data.data() + data.size()))
    {
        msg.payload.assign(ptr, ptr + ddsMsg->payloadSize);
    }

    // 从headers中提取topic
    auto it = msg.headers.find("topic");
    if (it != msg.headers.end())
    {
        msg.topic = it->second;
    }

    return msg;
}

void DDSProtocolAdapter::LogDDSError(int errorCode, const std::string &operation)
{
#ifdef USE_DDS
    const char *errorMsg = dds_strretcode(-errorCode);
    std::string fullError = "DDS error in " + operation + ": " + errorMsg +
                            " (code: " + std::to_string(errorCode) + ")";
    NotifyError(fullError, errorCode);
#endif
}

uint32_t DDSProtocolAdapter::CalculateChecksum(const std::vector<uint8_t> &data) const
{
    uint32_t checksum = 0;
    for (size_t i = 0; i < data.size(); ++i)
    {
        checksum += data[i];
    }
    return checksum;
}

void DDSProtocolAdapter::MaintenanceThreadFunc()
{
    while (!shouldStop_)
    {
        // 定期清理不活跃的客户端
#ifdef USE_DDS
        auto now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

        std::lock_guard<std::mutex> lock(clientsMutex_);
        std::vector<ClientId> clientsToRemove;

        for (const auto &pair : ddsClients_)
        {
            if (now - pair.second.lastSeenTime > GetConfig().connectionTimeoutMs)
            {
                clientsToRemove.push_back(pair.first);
            }
        }

        for (ClientId clientId : clientsToRemove)
        {
            DisconnectClient(clientId, "Inactive timeout");
        }
#endif

        std::this_thread::sleep_for(milliseconds(5000));
    }
}

} // namespace CommunicationModule