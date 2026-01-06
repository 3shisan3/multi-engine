#include "multi_protocol_communicate.h"
#include "communicate_hub_factory.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>

using namespace std::chrono;

namespace CommunicationModule
{

MultiProtocolCommunicationHub::MultiProtocolCommunicationHub()
    : defaultProtocol_("TCP")
{
    ResetStatistics();
}

MultiProtocolCommunicationHub::~MultiProtocolCommunicationHub()
{
    Shutdown();
}

void MultiProtocolCommunicationHub::ResetStatistics()
{
    std::lock_guard<std::mutex> lock(statsMutex_);
    statistics_.clear();
    statistics_["total_messages_received"] = 0;
    statistics_["total_messages_sent"] = 0;
    statistics_["total_bytes_received"] = 0;
    statistics_["total_bytes_sent"] = 0;
    statistics_["clients_connected"] = 0;
    statistics_["clients_disconnected"] = 0;
    statistics_["broadcast_messages"] = 0;
    statistics_["topic_messages"] = 0;
    statistics_["protocol_adapters"] = 0;
    statistics_["data_receivers"] = 0;
    statistics_["errors"] = 0;
    statistics_["startup_time"] = 0;
}

bool MultiProtocolCommunicationHub::Initialize(const std::vector<CommunicationConfig> &configs)
{
    if (running_)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(adaptersMutex_);

    bool allSuccess = true;

    for (const auto &config : configs)
    {
        if (!InitializeProtocolAdapter(config))
        {
            if (errorCallback_)
            {
                errorCallback_("InitializeProtocolAdapter",
                               "Failed to initialize protocol adapter: " + config.protocol);
            }
            allSuccess = false;
        }
    }

    if (allSuccess && !protocolAdapters_.empty())
    {
        defaultProtocol_ = protocolAdapters_.begin()->first;

        std::lock_guard<std::mutex> statsLock(statsMutex_);
        statistics_["startup_time"] = duration_cast<milliseconds>(
                                          system_clock::now().time_since_epoch())
                                          .count();
    }

    return allSuccess;
}

bool MultiProtocolCommunicationHub::Start()
{
    if (running_)
    {
        return true;
    }

    // 启动所有协议适配器
    {
        std::lock_guard<std::mutex> lock(adaptersMutex_);
        for (auto &pair : protocolAdapters_)
        {
            StartProtocolAdapter(pair.first);
        }
    }

    // 启动消息分发器
    messageDispatcher_.SetErrorCallback([this](const std::string &error)
                                        {
        if (errorCallback_) {
            errorCallback_("MessageDispatcher", error);
        }
        UpdateStatistics("errors", 1); });

    running_ = true;

    // 启动维护线程
    try
    {
        maintenanceThread_ = std::thread(&MultiProtocolCommunicationHub::MaintenanceThreadFunc, this);
    }
    catch (const std::exception &e)
    {
        running_ = false;
        if (errorCallback_)
        {
            errorCallback_("Start",
                           std::string("Failed to start maintenance thread: ") + e.what());
        }
        return false;
    }

    return true;
}

void MultiProtocolCommunicationHub::Stop()
{
    if (!running_)
    {
        return;
    }

    running_ = false;

    // 通知维护线程
    maintenanceCV_.notify_all();

    // 等待维护线程结束
    if (maintenanceThread_.joinable())
    {
        maintenanceThread_.join();
    }

    // 停止所有协议适配器
    {
        std::lock_guard<std::mutex> lock(adaptersMutex_);
        for (auto &pair : protocolAdapters_)
        {
            StopProtocolAdapter(pair.first);
        }
    }
}

void MultiProtocolCommunicationHub::Shutdown()
{
    Stop();

    // 清理所有适配器
    {
        std::lock_guard<std::mutex> lock(adaptersMutex_);
        protocolAdapters_.clear();
    }

    // 清理客户端信息
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        connectedClients_.clear();
        protocolClients_.clear();
    }

    // 清理主题订阅
    {
        std::lock_guard<std::mutex> lock(topicsMutex_);
        topicSubscriptions_.clear();
        topicSubscribers_.clear();
    }

    // 清理消息分发器
    messageDispatcher_.SetErrorCallback(nullptr);

    errorCallback_ = nullptr;

    ResetStatistics();
}

bool MultiProtocolCommunicationHub::AddProtocolAdapter(const CommunicationConfig &config)
{
    if (running_)
    {
        // 运行时添加适配器需要特殊处理
        if (errorCallback_)
        {
            errorCallback_("AddProtocolAdapter",
                           "Cannot add protocol adapter while hub is running");
        }
        return false;
    }

    std::lock_guard<std::mutex> lock(adaptersMutex_);

    if (protocolAdapters_.find(config.protocol) != protocolAdapters_.end())
    {
        if (errorCallback_)
        {
            errorCallback_("AddProtocolAdapter",
                           "Protocol adapter already exists: " + config.protocol);
        }
        return false;
    }

    return InitializeProtocolAdapter(config);
}

bool MultiProtocolCommunicationHub::RemoveProtocolAdapter(const std::string &protocolType)
{
    std::lock_guard<std::mutex> lock(adaptersMutex_);

    auto it = protocolAdapters_.find(protocolType);
    if (it == protocolAdapters_.end())
    {
        if (errorCallback_)
        {
            errorCallback_("RemoveProtocolAdapter",
                           "Protocol adapter not found: " + protocolType);
        }
        return false;
    }

    // 停止适配器
    StopProtocolAdapter(protocolType);

    // 断开使用该协议的所有客户端
    {
        std::lock_guard<std::mutex> clientLock(clientsMutex_);
        auto clientIt = protocolClients_.find(protocolType);
        if (clientIt != protocolClients_.end())
        {
            for (ClientId clientId : clientIt->second)
            {
                DisconnectClient(clientId, "Protocol adapter removed");
            }
        }
    }

    // 移除适配器
    protocolAdapters_.erase(it);

    UpdateStatistics("protocol_adapters", -1);

    return true;
}

bool MultiProtocolCommunicationHub::HasProtocolAdapter(const std::string &protocolType) const
{
    std::lock_guard<std::mutex> lock(adaptersMutex_);
    return protocolAdapters_.find(protocolType) != protocolAdapters_.end();
}

bool MultiProtocolCommunicationHub::SendToClient(ClientId clientId, const NetworkMessage &message)
{
    if (!running_)
    {
        return false;
    }

    std::string protocolType;

    {
        std::lock_guard<std::mutex> lock(clientsMutex_);

        auto clientIt = connectedClients_.find(clientId);
        if (clientIt == connectedClients_.end())
        {
            if (errorCallback_)
            {
                errorCallback_("SendToClient",
                               "Client not found: " + std::to_string(clientId));
            }
            return false;
        }

        protocolType = clientIt->second->protocolType;
    }

    return SendViaProtocol(protocolType, clientId, message);
}

int MultiProtocolCommunicationHub::Broadcast(const NetworkMessage &message)
{
    if (!running_)
    {
        return 0;
    }

    std::lock_guard<std::mutex> lock(clientsMutex_);

    int totalSuccess = 0;
    std::unordered_map<std::string, std::vector<ClientId>> clientsByProtocol;

    // 按协议分组客户端
    for (const auto &pair : connectedClients_)
    {
        clientsByProtocol[pair.second->protocolType].push_back(pair.first);
    }

    // 对每个协议批量发送
    for (const auto &protocolPair : clientsByProtocol)
    {
        const auto &protocolType = protocolPair.first;
        const auto &clientIds = protocolPair.second;

        for (ClientId clientId : clientIds)
        {
            if (SendViaProtocol(protocolType, clientId, message))
            {
                totalSuccess++;
            }
        }
    }

    if (totalSuccess > 0)
    {
        UpdateStatistics("broadcast_messages", 1);
        UpdateStatistics("total_messages_sent", totalSuccess);
        UpdateStatistics("total_bytes_sent", message.payload.size() * totalSuccess);
    }

    return totalSuccess;
}

int MultiProtocolCommunicationHub::PublishToTopic(const std::string &topic, const NetworkMessage &message)
{
    if (!running_ || topic.empty())
    {
        return 0;
    }

    std::vector<ClientId> subscribers;

    {
        std::lock_guard<std::mutex> lock(clientsMutex_);

        auto topicIt = topicSubscribers_.find(topic);
        if (topicIt == topicSubscribers_.end())
        {
            return 0;
        }

        subscribers.assign(topicIt->second.begin(), topicIt->second.end());
    }

    int totalSuccess = 0;

    for (ClientId clientId : subscribers)
    {
        NetworkMessage msgCopy = message;
        msgCopy.topic = topic;

        if (SendToClient(clientId, msgCopy))
        {
            totalSuccess++;
        }
    }

    // 调用主题回调
    {
        std::lock_guard<std::mutex> topicLock(topicsMutex_);
        auto subscriptionIt = topicSubscriptions_.find(topic);
        if (subscriptionIt != topicSubscriptions_.end())
        {
            std::lock_guard<std::mutex> callbackLock(subscriptionIt->second->mutex);
            for (const auto &callback : subscriptionIt->second->callbacks)
            {
                try
                {
                    ConnectionContext context;
                    context.clientId = 0;
                    context.clientType = ClientType::INTERNAL_MODULE;
                    context.clientName = "InternalPublisher";
                    callback(context, message);
                }
                catch (const std::exception &e)
                {
                    if (errorCallback_)
                    {
                        errorCallback_("PublishToTopic",
                                       std::string("Error in topic callback: ") + e.what());
                    }
                }
            }
        }
    }

    if (totalSuccess > 0)
    {
        UpdateStatistics("topic_messages", 1);
    }

    return totalSuccess;
}

bool MultiProtocolCommunicationHub::SendViaProtocol(const std::string &protocolType,
                                                    ClientId clientId,
                                                    const NetworkMessage &message)
{
    if (!running_)
    {
        return false;
    }

    std::lock_guard<std::mutex> adapterLock(adaptersMutex_);

    auto adapterIt = protocolAdapters_.find(protocolType);
    if (adapterIt == protocolAdapters_.end())
    {
        if (errorCallback_)
        {
            errorCallback_("SendViaProtocol",
                           "Protocol adapter not found: " + protocolType);
        }
        return false;
    }

    if (!adapterIt->second.isRunning)
    {
        if (errorCallback_)
        {
            errorCallback_("SendViaProtocol",
                           "Protocol adapter not running: " + protocolType);
        }
        return false;
    }

    bool success = adapterIt->second.adapter->SendToClient(clientId, message);
    if (success)
    {
        UpdateStatistics("total_messages_sent", 1);
        UpdateStatistics("total_bytes_sent", message.payload.size());
    }

    return success;
}

bool MultiProtocolCommunicationHub::RegisterDataReceiver(IDataReceiver *receiver)
{
    if (!receiver)
    {
        return false;
    }

    bool success = messageDispatcher_.AddReceiver(receiver);
    if (success)
    {
        UpdateStatistics("data_receivers", 1);

        // 初始化接收器
        try
        {
            if (!receiver->Initialize())
            {
                messageDispatcher_.RemoveReceiver(receiver);
                UpdateStatistics("data_receivers", -1);
                return false;
            }
        }
        catch (const std::exception &e)
        {
            if (errorCallback_)
            {
                errorCallback_("RegisterDataReceiver",
                               std::string("Failed to initialize receiver: ") + e.what());
            }
            messageDispatcher_.RemoveReceiver(receiver);
            UpdateStatistics("data_receivers", -1);
            return false;
        }
    }

    return success;
}

bool MultiProtocolCommunicationHub::UnregisterDataReceiver(IDataReceiver *receiver)
{
    if (!receiver)
    {
        return false;
    }

    bool success = messageDispatcher_.RemoveReceiver(receiver);
    if (success)
    {
        UpdateStatistics("data_receivers", -1);

        // 清理接收器
        try
        {
            receiver->Cleanup();
        }
        catch (const std::exception &e)
        {
            if (errorCallback_)
            {
                errorCallback_("UnregisterDataReceiver",
                               std::string("Error cleaning up receiver: ") + e.what());
            }
        }
    }

    return success;
}

std::vector<ConnectionContext> MultiProtocolCommunicationHub::GetConnectedClients() const
{
    std::lock_guard<std::mutex> lock(clientsMutex_);
    std::vector<ConnectionContext> clients;
    clients.reserve(connectedClients_.size());

    for (const auto &pair : connectedClients_)
    {
        clients.push_back(*pair.second->context);
    }

    return clients;
}

std::shared_ptr<ConnectionContext> MultiProtocolCommunicationHub::GetClientContext(ClientId clientId) const
{
    std::lock_guard<std::mutex> lock(clientsMutex_);
    auto it = connectedClients_.find(clientId);
    if (it != connectedClients_.end())
    {
        return it->second->context;
    }
    return nullptr;
}

std::vector<std::shared_ptr<ConnectionContext>> MultiProtocolCommunicationHub::FindClients(
    std::function<bool(const ConnectionContext &)> predicate) const
{
    std::lock_guard<std::mutex> lock(clientsMutex_);
    std::vector<std::shared_ptr<ConnectionContext>> result;

    for (const auto &pair : connectedClients_)
    {
        if (predicate(*pair.second->context))
        {
            result.push_back(pair.second->context);
        }
    }

    return result;
}

bool MultiProtocolCommunicationHub::DisconnectClient(ClientId clientId, const std::string &reason)
{
    if (!running_)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(clientsMutex_);

    auto clientIt = connectedClients_.find(clientId);
    if (clientIt == connectedClients_.end())
    {
        return false;
    }

    const auto &clientInfo = clientIt->second;

    // 从协议客户端列表中移除
    auto protocolIt = protocolClients_.find(clientInfo->protocolType);
    if (protocolIt != protocolClients_.end())
    {
        protocolIt->second.erase(clientId);
    }

    // 从所有主题订阅中移除
    for (auto &topicPair : topicSubscribers_)
    {
        topicPair.second.erase(clientId);
    }

    // 调用协议适配器断开连接
    {
        std::lock_guard<std::mutex> adapterLock(adaptersMutex_);
        auto adapterIt = protocolAdapters_.find(clientInfo->protocolType);
        if (adapterIt != protocolAdapters_.end())
        {
            adapterIt->second.adapter->DisconnectClient(clientId, reason);
        }
    }

    // 移除客户端
    connectedClients_.erase(clientIt);

    UpdateStatistics("clients_disconnected", 1);

    return true;
}

bool MultiProtocolCommunicationHub::SubscribeToTopic(const std::string &topic,
                                                     std::function<void(const ConnectionContext &,
                                                                        const NetworkMessage &)>
                                                         callback)
{
    if (topic.empty() || !callback)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(topicsMutex_);

    auto it = topicSubscriptions_.find(topic);
    if (it == topicSubscriptions_.end())
    {
        auto subscription = std::make_shared<TopicSubscription>();
        subscription->topic = topic;
        subscription->callbacks.push_back(callback);
        topicSubscriptions_[topic] = subscription;
    }
    else
    {
        std::lock_guard<std::mutex> callbackLock(it->second->mutex);
        it->second->callbacks.push_back(callback);
    }

    return true;
}

bool MultiProtocolCommunicationHub::UnsubscribeFromTopic(const std::string &topic)
{
    if (topic.empty())
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(topicsMutex_);

    auto it = topicSubscriptions_.find(topic);
    if (it == topicSubscriptions_.end())
    {
        return false;
    }

    topicSubscriptions_.erase(it);

    // 清理订阅者列表
    {
        std::lock_guard<std::mutex> clientLock(clientsMutex_);
        topicSubscribers_.erase(topic);
    }

    return true;
}

std::map<std::string, uint64_t> MultiProtocolCommunicationHub::GetStatistics() const
{
    std::lock_guard<std::mutex> lock(statsMutex_);
    std::map<std::string, uint64_t> allStats = statistics_;

    // 合并所有协议适配器的统计信息
    std::lock_guard<std::mutex> adapterLock(adaptersMutex_);
    for (const auto &pair : protocolAdapters_)
    {
        auto adapterStats = pair.second.adapter->GetStatistics();
        for (const auto &statPair : adapterStats)
        {
            std::string key = pair.first + "_" + statPair.first;
            allStats[key] = statPair.second;
        }
    }

    // 添加动态统计
    {
        std::lock_guard<std::mutex> clientLock(clientsMutex_);
        allStats["connected_clients"] = connectedClients_.size();
        allStats["protocol_count"] = protocolClients_.size();
    }

    {
        std::lock_guard<std::mutex> topicLock(topicsMutex_);
        allStats["topics"] = topicSubscriptions_.size();

        size_t totalSubscribers = 0;
        for (const auto &pair : topicSubscribers_)
        {
            totalSubscribers += pair.second.size();
        }
        allStats["topic_subscribers"] = totalSubscribers;
    }

    allStats["data_receivers"] = messageDispatcher_.GetReceivers().size();

    return allStats;
}

std::vector<std::string> MultiProtocolCommunicationHub::GetSupportedProtocols() const
{
    std::lock_guard<std::mutex> lock(adaptersMutex_);
    std::vector<std::string> protocols;
    protocols.reserve(protocolAdapters_.size());

    for (const auto &pair : protocolAdapters_)
    {
        protocols.push_back(pair.first);
    }

    return protocols;
}

std::map<std::string, bool> MultiProtocolCommunicationHub::GetProtocolStatus() const
{
    std::lock_guard<std::mutex> lock(adaptersMutex_);
    std::map<std::string, bool> status;

    for (const auto &pair : protocolAdapters_)
    {
        status[pair.first] = pair.second.isRunning;
    }

    return status;
}

void MultiProtocolCommunicationHub::SetErrorCallback(
    std::function<void(const std::string &, const std::string &)> callback)
{
    errorCallback_ = callback;
}

std::vector<std::string> MultiProtocolCommunicationHub::GetRegisteredReceivers() const
{
    return messageDispatcher_.GetReceiverNames();
}

int MultiProtocolCommunicationHub::SendToClientGroup(const std::vector<ClientId> &clientIds,
                                                     const NetworkMessage &message)
{
    if (!running_ || clientIds.empty())
    {
        return 0;
    }

    int successCount = 0;

    for (ClientId clientId : clientIds)
    {
        if (SendToClient(clientId, message))
        {
            successCount++;
        }
    }

    return successCount;
}

int MultiProtocolCommunicationHub::SendToClientType(ClientType clientType, const NetworkMessage &message)
{
    if (!running_)
    {
        return 0;
    }

    auto clients = FindClients([clientType](const ConnectionContext &context)
                               { return context.clientType == clientType; });

    int successCount = 0;

    for (const auto &clientContext : clients)
    {
        if (SendToClient(clientContext->clientId, message))
        {
            successCount++;
        }
    }

    return successCount;
}

std::vector<std::string> MultiProtocolCommunicationHub::GetClientSubscribedTopics(ClientId clientId) const
{
    std::lock_guard<std::mutex> lock(clientsMutex_);

    auto it = connectedClients_.find(clientId);
    if (it == connectedClients_.end())
    {
        return {};
    }

    return std::vector<std::string>(
        it->second->subscribedTopics.begin(),
        it->second->subscribedTopics.end());
}

bool MultiProtocolCommunicationHub::IsClientSubscribedToTopic(ClientId clientId, const std::string &topic) const
{
    std::lock_guard<std::mutex> lock(clientsMutex_);

    auto it = connectedClients_.find(clientId);
    if (it == connectedClients_.end())
    {
        return false;
    }

    return it->second->subscribedTopics.find(topic) != it->second->subscribedTopics.end();
}

std::vector<ClientId> MultiProtocolCommunicationHub::GetTopicSubscribers(const std::string &topic) const
{
    std::lock_guard<std::mutex> lock(clientsMutex_);

    auto it = topicSubscribers_.find(topic);
    if (it == topicSubscribers_.end())
    {
        return {};
    }

    return std::vector<ClientId>(it->second.begin(), it->second.end());
}

// =============== 私有方法实现 ===============

bool MultiProtocolCommunicationHub::InitializeProtocolAdapter(const CommunicationConfig &config)
{
    auto adapter = CommunicationHubFactory::CreateProtocolAdapter(config.protocol);
    if (!adapter)
    {
        return false;
    }

    if (!adapter->Initialize(config))
    {
        return false;
    }

    // 设置回调
    adapter->SetMessageCallback([this, protocol = config.protocol](
                                    const ConnectionContext &context, const NetworkMessage &message)
                                { OnProtocolMessageReceived(protocol, context, message); });

    adapter->SetConnectionCallback([this, protocol = config.protocol](
                                       const ConnectionContext &context, bool connected)
                                   { OnProtocolConnectionChanged(protocol, context, connected); });

    adapter->SetErrorCallback([this, protocol = config.protocol](
                                  const std::string &error, int code)
                              { OnProtocolError(protocol, error, code); });

    // 使用构造函数创建 ProtocolAdapterInfo
    ProtocolAdapterInfo info(std::move(adapter), config, false, 
                           config.protocol + "_Adapter");

    // 直接插入，避免移动赋值问题
    protocolAdapters_.emplace(config.protocol, std::move(info));

    UpdateStatistics("protocol_adapters", 1);

    return true;
}

void MultiProtocolCommunicationHub::StartProtocolAdapter(const std::string &protocolType)
{
    auto it = protocolAdapters_.find(protocolType);
    if (it == protocolAdapters_.end() || it->second.isRunning)
    {
        return;
    }

    if (it->second.adapter->Start())
    {
        it->second.isRunning = true;
    }
}

void MultiProtocolCommunicationHub::StopProtocolAdapter(const std::string &protocolType)
{
    auto it = protocolAdapters_.find(protocolType);
    if (it == protocolAdapters_.end() || !it->second.isRunning)
    {
        return;
    }

    it->second.adapter->Stop();
    it->second.isRunning = false;
}

void MultiProtocolCommunicationHub::OnProtocolMessageReceived(const std::string &protocolType,
                                                              const ConnectionContext &context,
                                                              const NetworkMessage &message)
{
    // 更新统计信息
    UpdateStatistics("total_messages_received", 1);
    UpdateStatistics("total_bytes_received", message.payload.size());

    // 处理消息
    HandleIncomingMessage(protocolType, context, message);
}

void MultiProtocolCommunicationHub::OnProtocolConnectionChanged(const std::string &protocolType,
                                                                const ConnectionContext &context,
                                                                bool connected)
{
    std::lock_guard<std::mutex> lock(clientsMutex_);

    if (connected)
    {
        // 新客户端连接
        ClientId clientId = context.clientId;
        if (clientId == 0)
        {
            clientId = nextClientId_++;
        }

        auto clientInfo = std::make_shared<ClientInfo>();
        clientInfo->context = std::make_shared<ConnectionContext>(context);
        clientInfo->context->clientId = clientId;
        clientInfo->protocolType = protocolType;
        clientInfo->lastActivityTime = duration_cast<milliseconds>(
                                           system_clock::now().time_since_epoch())
                                           .count();

        connectedClients_[clientId] = clientInfo;
        protocolClients_[protocolType].insert(clientId);

        UpdateStatistics("clients_connected", 1);
    }
    else
    {
        // 客户端断开连接
        auto clientIt = connectedClients_.find(context.clientId);
        if (clientIt != connectedClients_.end())
        {
            // 从协议客户端列表中移除
            auto protocolIt = protocolClients_.find(protocolType);
            if (protocolIt != protocolClients_.end())
            {
                protocolIt->second.erase(context.clientId);
            }

            // 从所有主题订阅中移除
            for (auto &topicPair : topicSubscribers_)
            {
                topicPair.second.erase(context.clientId);
            }

            // 移除客户端
            connectedClients_.erase(clientIt);

            UpdateStatistics("clients_disconnected", 1);
        }
    }
}

void MultiProtocolCommunicationHub::OnProtocolError(const std::string &protocolType,
                                                    const std::string &errorMessage,
                                                    int errorCode)
{
    if (errorCallback_)
    {
        errorCallback_(protocolType + "Adapter",
                       errorMessage + " (code: " + std::to_string(errorCode) + ")");
    }

    UpdateStatistics("errors", 1);
}

void MultiProtocolCommunicationHub::HandleIncomingMessage(const std::string &protocolType,
                                                          const ConnectionContext &context,
                                                          const NetworkMessage &message)
{
    // 更新客户端活动时间
    UpdateClientActivity(context.clientId);

    // 处理主题订阅/取消订阅
    if (!message.topic.empty())
    {
        std::lock_guard<std::mutex> lock(clientsMutex_);

        auto clientIt = connectedClients_.find(context.clientId);
        if (clientIt != connectedClients_.end())
        {
            clientIt->second->subscribedTopics.insert(message.topic);
            topicSubscribers_[message.topic].insert(context.clientId);
        }
    }

    // 检查控制命令
    auto command = message.GetString("command");
    if (!command.empty())
    {
        if (command == "subscribe")
        {
            auto topic = message.GetString("topic");
            if (!topic.empty())
            {
                std::lock_guard<std::mutex> lock(clientsMutex_);

                auto clientIt = connectedClients_.find(context.clientId);
                if (clientIt != connectedClients_.end())
                {
                    clientIt->second->subscribedTopics.insert(topic);
                    topicSubscribers_[topic].insert(context.clientId);
                }
            }
        }
        else if (command == "unsubscribe")
        {
            auto topic = message.GetString("topic");
            if (!topic.empty())
            {
                std::lock_guard<std::mutex> lock(clientsMutex_);

                auto clientIt = connectedClients_.find(context.clientId);
                if (clientIt != connectedClients_.end())
                {
                    clientIt->second->subscribedTopics.erase(topic);
                }

                auto topicIt = topicSubscribers_.find(topic);
                if (topicIt != topicSubscribers_.end())
                {
                    topicIt->second.erase(context.clientId);
                }
            }
        }
    }

    // 调用主题回调
    if (!message.topic.empty())
    {
        std::lock_guard<std::mutex> lock(topicsMutex_);
        auto subscriptionIt = topicSubscriptions_.find(message.topic);
        if (subscriptionIt != topicSubscriptions_.end())
        {
            std::lock_guard<std::mutex> callbackLock(subscriptionIt->second->mutex);
            for (const auto &callback : subscriptionIt->second->callbacks)
            {
                try
                {
                    callback(context, message);
                }
                catch (const std::exception &e)
                {
                    if (errorCallback_)
                    {
                        errorCallback_("HandleIncomingMessage",
                                       std::string("Error in topic callback: ") + e.what());
                    }
                }
            }
        }
    }

    // 分发消息给数据接收器
    messageDispatcher_.DispatchMessage(context, message);
}

void MultiProtocolCommunicationHub::UpdateClientActivity(ClientId clientId)
{
    std::lock_guard<std::mutex> lock(clientsMutex_);
    auto it = connectedClients_.find(clientId);
    if (it != connectedClients_.end())
    {
        it->second->lastActivityTime = duration_cast<milliseconds>(
                                           system_clock::now().time_since_epoch())
                                           .count();
    }
}

void MultiProtocolCommunicationHub::CleanupInactiveClients()
{
    auto now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    std::lock_guard<std::mutex> lock(clientsMutex_);
    std::vector<ClientId> clientsToDisconnect;

    for (const auto &pair : connectedClients_)
    {
        if (now - pair.second->lastActivityTime > connectionTimeoutMs_)
        {
            clientsToDisconnect.push_back(pair.first);
        }
    }

    for (ClientId clientId : clientsToDisconnect)
    {
        DisconnectClient(clientId, "Inactive timeout");
    }
}

std::string MultiProtocolCommunicationHub::SelectProtocolForClient(ClientId clientId) const
{
    std::lock_guard<std::mutex> lock(clientsMutex_);

    auto clientIt = connectedClients_.find(clientId);
    if (clientIt != connectedClients_.end())
    {
        return clientIt->second->protocolType;
    }

    return defaultProtocol_;
}

void MultiProtocolCommunicationHub::MaintenanceThreadFunc()
{
    while (running_)
    {
        // 清理不活跃的客户端
        CleanupInactiveClients();

        // 更新统计信息
        {
            std::lock_guard<std::mutex> lock(clientsMutex_);
            UpdateStatistics("active_clients", connectedClients_.size());
        }

        // 等待下一次维护
        std::unique_lock<std::mutex> lock(adaptersMutex_);
        maintenanceCV_.wait_for(lock, milliseconds(maintenanceIntervalMs_),
                                [this]()
                                { return !running_; });
    }
}

void MultiProtocolCommunicationHub::UpdateStatistics(const std::string &key, uint64_t delta)
{
    std::lock_guard<std::mutex> lock(statsMutex_);
    statistics_[key] += delta;
}

// =============== MessageDispatcher 实现 ===============

MultiProtocolCommunicationHub::MessageDispatcher::~MessageDispatcher()
{
    // 清理所有接收器
    std::lock_guard<std::mutex> lock(receiversMutex_);
    for (auto &pair : receivers_)
    {
        try
        {
            pair.second->Cleanup();
        }
        catch (...)
        {
            // 忽略清理异常
        }
    }
    receivers_.clear();
}

bool MultiProtocolCommunicationHub::MessageDispatcher::AddReceiver(IDataReceiver *receiver)
{
    if (!receiver)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(receiversMutex_);

    std::string name = receiver->GetReceiverName();
    if (name.empty() || receivers_.find(name) != receivers_.end())
    {
        return false;
    }

    receivers_[name] = receiver;
    return true;
}

bool MultiProtocolCommunicationHub::MessageDispatcher::RemoveReceiver(IDataReceiver *receiver)
{
    if (!receiver)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(receiversMutex_);

    for (auto it = receivers_.begin(); it != receivers_.end(); ++it)
    {
        if (it->second == receiver)
        {
            receivers_.erase(it);
            return true;
        }
    }

    return false;
}

void MultiProtocolCommunicationHub::MessageDispatcher::DispatchMessage(
    const ConnectionContext &context,
    const NetworkMessage &message)
{
    std::vector<IDataReceiver *> receiversCopy;

    {
        std::lock_guard<std::mutex> lock(receiversMutex_);
        receiversCopy.reserve(receivers_.size());
        for (const auto &pair : receivers_)
        {
            receiversCopy.push_back(pair.second);
        }
    }

    for (auto *receiver : receiversCopy)
    {
        try
        {
            bool shouldProcess = false;

            // 检查接收器是否对该消息类型感兴趣
            auto interestedTypes = receiver->GetInterestedMessageTypes();
            if (std::find(interestedTypes.begin(), interestedTypes.end(), message.type) != interestedTypes.end())
            {
                shouldProcess = true;
            }

            // 检查接收器是否订阅了该主题
            if (!message.topic.empty())
            {
                auto subscribedTopics = receiver->GetSubscribedTopics();
                if (std::find(subscribedTopics.begin(), subscribedTopics.end(), message.topic) != subscribedTopics.end())
                {
                    shouldProcess = true;
                }
            }

            if (shouldProcess)
            {
                receiver->OnDataReceived(context, message);
            }
        }
        catch (const std::exception &e)
        {
            if (errorCallback_)
            {
                errorCallback_(std::string("Error in receiver ") + receiver->GetReceiverName() + ": " + e.what());
            }
        }
    }
}

std::vector<IDataReceiver *> MultiProtocolCommunicationHub::MessageDispatcher::GetReceivers() const
{
    std::lock_guard<std::mutex> lock(receiversMutex_);
    std::vector<IDataReceiver *> receivers;
    receivers.reserve(receivers_.size());

    for (const auto &pair : receivers_)
    {
        receivers.push_back(pair.second);
    }

    return receivers;
}

std::vector<std::string> MultiProtocolCommunicationHub::MessageDispatcher::GetReceiverNames() const
{
    std::lock_guard<std::mutex> lock(receiversMutex_);
    std::vector<std::string> names;
    names.reserve(receivers_.size());

    for (const auto &pair : receivers_)
    {
        names.push_back(pair.first);
    }

    return names;
}

void MultiProtocolCommunicationHub::MessageDispatcher::SetErrorCallback(
    std::function<void(const std::string &)> callback)
{
    errorCallback_ = callback;
}

} // namespace CommunicationModule