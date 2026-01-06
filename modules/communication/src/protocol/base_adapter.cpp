#include "protocol/base_adapter.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace std::chrono;

namespace CommunicationModule
{

BaseProtocolAdapter::BaseProtocolAdapter(const std::string &protocolType,
                                         const std::string &adapterName)
    : protocolType_(protocolType), adapterName_(adapterName)
{
    ResetStatistics();
}

BaseProtocolAdapter::~BaseProtocolAdapter()
{
    Stop();
}

bool BaseProtocolAdapter::Initialize(const CommunicationConfig &config)
{
    if (isInitialized_)
    {
        return true;
    }

    config_ = config;

    // 初始化统计
    ResetStatistics();

    // 调用子类初始化
    if (!DoInitialize())
    {
        return false;
    }

    isInitialized_ = true;
    return true;
}

bool BaseProtocolAdapter::Start()
{
    if (isRunning_ || !isInitialized_)
    {
        return false;
    }

    // 调用子类启动
    if (!DoStart())
    {
        return false;
    }

    isRunning_ = true;

    // 更新统计
    UpdateStatistic("startup_time",
                    duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());

    return true;
}

void BaseProtocolAdapter::Stop()
{
    if (!isRunning_)
    {
        return;
    }

    // 调用子类停止
    DoStop();

    isRunning_ = false;

    // 更新统计
    UpdateStatistic("shutdown_time",
                    duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

std::map<std::string, uint64_t> BaseProtocolAdapter::GetStatistics() const
{
    std::lock_guard<std::mutex> lock(statsMutex_);
    return statistics_;
}

void BaseProtocolAdapter::ResetStatistics()
{
    std::lock_guard<std::mutex> lock(statsMutex_);
    statistics_.clear();
    statistics_["messages_sent"] = 0;
    statistics_["messages_received"] = 0;
    statistics_["bytes_sent"] = 0;
    statistics_["bytes_received"] = 0;
    statistics_["connections"] = 0;
    statistics_["disconnections"] = 0;
    statistics_["errors"] = 0;
    statistics_["startup_time"] = 0;
    statistics_["shutdown_time"] = 0;
}

void BaseProtocolAdapter::SetMessageCallback(
    std::function<void(const ConnectionContext &, const NetworkMessage &)> callback)
{
    messageCallback_ = callback;
}

void BaseProtocolAdapter::SetConnectionCallback(
    std::function<void(const ConnectionContext &, bool connected)> callback)
{
    connectionCallback_ = callback;
}

void BaseProtocolAdapter::SetErrorCallback(std::function<void(const std::string &, int)> callback)
{
    errorCallback_ = callback;
}

void BaseProtocolAdapter::NotifyMessageReceived(const ConnectionContext &context,
                                                const NetworkMessage &message)
{
    if (messageCallback_)
    {
        try
        {
            messageCallback_(context, message);
        }
        catch (const std::exception &e)
        {
            NotifyError(std::string("Error in message callback: ") + e.what());
        }
    }
}

void BaseProtocolAdapter::NotifyConnectionChanged(const ConnectionContext &context,
                                                  bool connected)
{
    if (connectionCallback_)
    {
        try
        {
            connectionCallback_(context, connected);
        }
        catch (const std::exception &e)
        {
            NotifyError(std::string("Error in connection callback: ") + e.what());
        }
    }
}

void BaseProtocolAdapter::NotifyError(const std::string &errorMessage, int errorCode)
{
    if (errorCallback_)
    {
        try
        {
            errorCallback_(errorMessage, errorCode);
        }
        catch (...)
        {
            // 忽略错误回调本身的异常
        }
    }

    UpdateStatistic("errors", 1);
}

void BaseProtocolAdapter::UpdateStatistic(const std::string &key, uint64_t delta)
{
    std::lock_guard<std::mutex> lock(statsMutex_);
    statistics_[key] += delta;
}

ClientId BaseProtocolAdapter::GetNextClientId()
{
    return nextClientId_++;
}

ConnectionContext BaseProtocolAdapter::CreateConnectionContext(ClientId clientId,
                                                               const std::string &remoteAddress,
                                                               int remotePort) const
{
    ConnectionContext context;
    context.clientId = clientId;
    context.protocolType = protocolType_;
    context.ipAddress = remoteAddress;
    context.port = remotePort;
    context.isConnected = true;
    context.lastHeartbeatTime = duration_cast<milliseconds>(
                                    system_clock::now().time_since_epoch())
                                    .count();

    // 从配置中提取信息
    if (config_.protocolParams.find("client_name_prefix") != config_.protocolParams.end())
    {
        context.clientName = config_.protocolParams.at("client_name_prefix") +
                             std::to_string(clientId);
    }
    else
    {
        context.clientName = protocolType_ + "_Client_" + std::to_string(clientId);
    }

    return context;
}

std::vector<uint8_t> BaseProtocolAdapter::SerializeMessage(const NetworkMessage &message) const
{
    // 基础序列化实现，子类可以覆盖
    // 这里简单地将payload复制过去
    return message.payload;
}

NetworkMessage BaseProtocolAdapter::DeserializeMessage(const std::vector<uint8_t> &data) const
{
    // 基础反序列化实现，子类可以覆盖
    NetworkMessage message;
    message.payload = data;
    message.timestamp = duration_cast<milliseconds>(
                            system_clock::now().time_since_epoch())
                            .count();
    return message;
}

void BaseProtocolAdapter::AddMessageHeaders(NetworkMessage &message,
                                            const std::map<std::string, std::string> &additionalHeaders) const
{
    // 添加协议头
    message.headers["protocol"] = protocolType_;
    message.headers["adapter"] = adapterName_;
    message.headers["timestamp"] = std::to_string(message.timestamp);

    // 添加额外头
    for (const auto &header : additionalHeaders)
    {
        message.headers[header.first] = header.second;
    }
}

std::map<std::string, std::string> BaseProtocolAdapter::ParseMessageHeaders(
    const std::vector<uint8_t> &data) const
{
    // 基础实现，子类需要根据具体协议格式实现
    return {};
}

} // namespace CommunicationModule