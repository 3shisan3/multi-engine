#include "protocol/udp_adapter.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <cstring>

#ifdef USE_ASIO
#include <asio/read.hpp>
#include <asio/write.hpp>
#endif

using namespace std::chrono;

namespace CommunicationModule
{

UDPProtocolAdapter::UDPProtocolAdapter()
    : BaseProtocolAdapter("UDP", "UDPProtocolAdapter")
    , localAddress_("0.0.0.0")
    , localPort_(8888)
    , numIOThreads_(2)
    , multicastTTL_(1)
    , enableBroadcast_(false)
    , receiveBufferSize_(65507)
    , sendBufferSize_(65507)
{
}

UDPProtocolAdapter::~UDPProtocolAdapter()
{
    DoStop();
    DoCleanup();
}

bool UDPProtocolAdapter::DoInitialize()
{
#ifdef USE_ASIO
    // 解析配置参数
    auto addressIt = GetConfig().protocolParams.find("local_address");
    if (addressIt != GetConfig().protocolParams.end())
    {
        localAddress_ = addressIt->second;
    }
    else if (!GetConfig().serverAddress.empty())
    {
        localAddress_ = GetConfig().serverAddress;
    }

    auto portIt = GetConfig().protocolParams.find("local_port");
    if (portIt != GetConfig().protocolParams.end())
    {
        localPort_ = std::stoi(portIt->second);
    }
    else if (GetConfig().serverPort > 0)
    {
        localPort_ = GetConfig().serverPort;
    }

    auto threadsIt = GetConfig().protocolParams.find("io_threads");
    if (threadsIt != GetConfig().protocolParams.end())
    {
        numIOThreads_ = std::stoi(threadsIt->second);
        if (numIOThreads_ <= 0 || numIOThreads_ > 16)
        {
            numIOThreads_ = 2;
        }
    }

    auto broadcastIt = GetConfig().protocolParams.find("enable_broadcast");
    if (broadcastIt != GetConfig().protocolParams.end())
    {
        enableBroadcast_ = (broadcastIt->second == "true" || broadcastIt->second == "1");
    }

    auto multicastIt = GetConfig().protocolParams.find("multicast_group");
    if (multicastIt != GetConfig().protocolParams.end())
    {
        multicastGroup_ = multicastIt->second;
    }

    auto ttlIt = GetConfig().protocolParams.find("multicast_ttl");
    if (ttlIt != GetConfig().protocolParams.end())
    {
        multicastTTL_ = std::stoi(ttlIt->second);
    }

    // 创建IO上下文
    io_context_ = std::make_shared<asio::io_context>();
    work_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
        asio::make_work_guard(*io_context_));

    // 创建UDP服务器
    udpServer_ = std::make_unique<UDPServer>(*io_context_, localAddress_, localPort_);

    // 设置服务器参数
    if (!multicastGroup_.empty())
    {
        udpServer_->SetMulticastGroup(multicastGroup_, multicastTTL_);
    }
    
    udpServer_->EnableBroadcast(enableBroadcast_);

    // 设置回调
    udpServer_->SetMessageCallback([this](const ConnectionContext& context, 
                                          const NetworkMessage& message) {
        NotifyMessageReceived(context, message);
        packetsReceived_.fetch_add(1);
        UpdateStatistic("messages_received", 1);
        UpdateStatistic("bytes_received", message.payload.size());
    });

    udpServer_->SetConnectionCallback([this](const ConnectionContext& context, bool connected) {
        NotifyConnectionChanged(context, connected);
        if (connected)
        {
            UpdateStatistic("endpoints_registered", 1);
        }
        else
        {
            UpdateStatistic("endpoints_unregistered", 1);
        }
    });

    udpServer_->SetErrorCallback([this](const std::string& error, int code) {
        NotifyError(error, code);
    });

    return true;
#else
    NotifyError("ASIO support not compiled in", -1);
    return false;
#endif
}

bool UDPProtocolAdapter::DoStart()
{
#ifdef USE_ASIO
    if (shouldStop_ || !io_context_ || !udpServer_)
    {
        return false;
    }

    // 启动IO线程
    try
    {
        for (int i = 0; i < numIOThreads_; ++i)
        {
            io_threads_.emplace_back(&UDPProtocolAdapter::IOContextThreadFunc, this);
        }
    }
    catch (const std::exception& e)
    {
        NotifyError(std::string("Failed to start IO threads: ") + e.what(), -2);
        return false;
    }

    // 启动UDP服务器
    udpServer_->Start();

    // 启动维护线程
    try
    {
        maintenanceThread_ = std::thread(&UDPProtocolAdapter::MaintenanceThreadFunc, this);
    }
    catch (const std::exception& e)
    {
        NotifyError(std::string("Failed to start maintenance thread: ") + e.what(), -3);
        return false;
    }

    return true;
#else
    return false;
#endif
}

void UDPProtocolAdapter::DoStop()
{
    shouldStop_ = true;

#ifdef USE_ASIO
    // 停止UDP服务器
    if (udpServer_)
    {
        udpServer_->Stop();
    }

    // 停止IO上下文
    if (work_)
    {
        work_.reset();
    }

    if (io_context_)
    {
        io_context_->stop();
    }
#endif

    // 等待线程结束
    if (maintenanceThread_.joinable())
    {
        maintenanceThread_.join();
    }

#ifdef USE_ASIO
    for (auto& thread : io_threads_)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }
    io_threads_.clear();
#endif
}

void UDPProtocolAdapter::DoCleanup()
{
#ifdef USE_ASIO
    udpServer_.reset();
    work_.reset();
    io_context_.reset();
#endif
}

bool UDPProtocolAdapter::SendToClient(ClientId clientId, const NetworkMessage& message)
{
#ifdef USE_ASIO
    if (!IsRunning() || !udpServer_)
    {
        return false;
    }

    auto data = SerializeUDPMessage(message);
    
    // UDP需要知道目标端点，这里简化处理
    // 实际应该根据clientId查找对应的端点
    asio::ip::udp::endpoint targetEndpoint(
        asio::ip::make_address("127.0.0.1"),  // 示例地址
        localPort_ + 1);                       // 示例端口
    
    // 简化实现，直接广播
    bool success = udpServer_->Broadcast(data, targetEndpoint);

    if (success)
    {
        packetsSent_.fetch_add(1);
        bytesSent_.fetch_add(data.size());
        UpdateStatistic("messages_sent", 1);
        UpdateStatistic("bytes_sent", data.size());
    }

    return success;
#else
    return false;
#endif
}

int UDPProtocolAdapter::Broadcast(const NetworkMessage &message)
{
#ifdef USE_ASIO
    if (!IsRunning() || !udpServer_)
    {
        return 0;
    }

    auto data = SerializeUDPMessage(message);
    
    // 创建广播端点
    asio::ip::udp::endpoint broadcastEndpoint(
        asio::ip::address_v4::broadcast(),
        localPort_ + 1);  // 假设客户端监听在localPort+1端口
    
    bool success = udpServer_->Broadcast(data, broadcastEndpoint);
    
    if (success)
    {
        packetsSent_.fetch_add(1);
        bytesSent_.fetch_add(data.size());
        UpdateStatistic("messages_sent", 1);
        UpdateStatistic("bytes_sent", data.size());
    }
    
    return success ? 1 : 0;
#else
    return 0;
#endif
}

int UDPProtocolAdapter::PublishToTopic(const std::string& topic, const NetworkMessage& message)
{
    // UDP不支持主题，通过消息头携带主题信息
    NetworkMessage msgWithTopic = message;
    msgWithTopic.topic = topic;
    msgWithTopic.headers["topic"] = topic;
    
    return Broadcast(msgWithTopic);
}

std::vector<ConnectionContext> UDPProtocolAdapter::GetConnectedClients() const
{
#ifdef USE_ASIO
    if (udpServer_)
    {
        return udpServer_->GetConnectedClients();
    }
#endif
    return {};
}

bool UDPProtocolAdapter::DisconnectClient(ClientId clientId, const std::string& reason)
{
#ifdef USE_ASIO
    // UDP是无连接的，这里只需要从活跃端点列表中移除
    // 实际实现应该在UDPServer中维护端点列表
    return true;
#else
    return false;
#endif
}

size_t UDPProtocolAdapter::GetConnectionCount() const
{
#ifdef USE_ASIO
    if (udpServer_)
    {
        return udpServer_->GetConnectionCount();
    }
#endif
    return 0;
}

size_t UDPProtocolAdapter::GetActiveConnectionCount() const
{
    // UDP没有连接概念，返回最近活跃的端点数量
    return GetConnectionCount();
}

bool UDPProtocolAdapter::SendRawData(ClientId clientId, const std::vector<uint8_t>& data)
{
#ifdef USE_ASIO
    if (!IsRunning() || !udpServer_)
    {
        return false;
    }

    // 创建目标端点（简化）
    asio::ip::udp::endpoint targetEndpoint(
        asio::ip::make_address("127.0.0.1"),
        localPort_ + 1);
    
    return udpServer_->Broadcast(data, targetEndpoint);
#else
    return false;
#endif
}

void UDPProtocolAdapter::SetMulticastGroup(const std::string& groupAddress, int ttl)
{
    multicastGroup_ = groupAddress;
    multicastTTL_ = ttl;
    
#ifdef USE_ASIO
    if (udpServer_)
    {
        udpServer_->SetMulticastGroup(groupAddress, ttl);
    }
#endif
}

void UDPProtocolAdapter::EnableBroadcast(bool enable)
{
    enableBroadcast_ = enable;
    
#ifdef USE_ASIO
    if (udpServer_)
    {
        udpServer_->EnableBroadcast(enable);
    }
#endif
}

void UDPProtocolAdapter::SetReceiveBufferSize(size_t size)
{
    receiveBufferSize_ = size;
}

void UDPProtocolAdapter::SetSendBufferSize(size_t size)
{
    sendBufferSize_ = size;
}

std::vector<uint8_t> UDPProtocolAdapter::SerializeUDPMessage(const NetworkMessage& msg) const
{
    // UDP消息格式: [消息类型(1字节)][主题长度(1字节)][主题][有效载荷]
    
    std::vector<uint8_t> buffer;
    
    // 消息类型
    buffer.push_back(static_cast<uint8_t>(msg.type));
    
    // 主题
    uint8_t topicLength = static_cast<uint8_t>(std::min(msg.topic.size(), size_t(255)));
    buffer.push_back(topicLength);
    if (topicLength > 0)
    {
        buffer.insert(buffer.end(), msg.topic.begin(), msg.topic.begin() + topicLength);
    }
    
    // 源客户端ID（8字节）
    uint64_t sourceId = msg.sourceClientId;
    for (int i = 0; i < 8; ++i)
    {
        buffer.push_back(static_cast<uint8_t>((sourceId >> (8 * i)) & 0xFF));
    }
    
    // 时间戳（8字节）
    uint64_t timestamp = msg.timestamp;
    for (int i = 0; i < 8; ++i)
    {
        buffer.push_back(static_cast<uint8_t>((timestamp >> (8 * i)) & 0xFF));
    }
    
    // 有效载荷
    if (!msg.payload.empty())
    {
        buffer.insert(buffer.end(), msg.payload.begin(), msg.payload.end());
    }
    
    return buffer;
}

NetworkMessage UDPProtocolAdapter::DeserializeUDPMessage(const std::vector<uint8_t>& data) const
{
    NetworkMessage msg;
    
    if (data.size() < 2)  // 至少需要类型和主题长度
    {
        return msg;
    }
    
    size_t offset = 0;
    
    // 消息类型
    msg.type = static_cast<MessageType>(data[offset++]);
    
    // 主题长度
    uint8_t topicLength = data[offset++];
    if (topicLength > 0 && offset + topicLength <= data.size())
    {
        msg.topic = std::string(reinterpret_cast<const char*>(&data[offset]), topicLength);
        offset += topicLength;
    }
    
    // 源客户端ID
    if (offset + 8 <= data.size())
    {
        msg.sourceClientId = 0;
        for (int i = 0; i < 8; ++i)
        {
            msg.sourceClientId |= static_cast<uint64_t>(data[offset++]) << (8 * i);
        }
    }
    
    // 时间戳
    if (offset + 8 <= data.size())
    {
        msg.timestamp = 0;
        for (int i = 0; i < 8; ++i)
        {
            msg.timestamp |= static_cast<uint64_t>(data[offset++]) << (8 * i);
        }
    }
    else
    {
        msg.timestamp = duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()).count();
    }
    
    // 有效载荷
    if (offset < data.size())
    {
        msg.payload.assign(data.begin() + offset, data.end());
    }
    
    return msg;
}

void UDPProtocolAdapter::IOContextThreadFunc()
{
#ifdef USE_ASIO
    while (!shouldStop_ && io_context_)
    {
        try
        {
            io_context_->run();
        }
        catch (const std::exception& e)
        {
            NotifyError(std::string("IO context error: ") + e.what(), -4);
        }
    }
#endif
}

void UDPProtocolAdapter::MaintenanceThreadFunc()
{
    while (!shouldStop_)
    {
        // 定期更新统计信息
        UpdateStatistic("active_endpoints", GetConnectionCount());
        
        // UDP不需要连接维护
        
        std::this_thread::sleep_for(milliseconds(1000));
    }
}

#ifdef USE_ASIO
// =============== UDPServer 实现 ===============

UDPProtocolAdapter::UDPServer::UDPServer(asio::io_context& io_context,
                                         const std::string& localAddress,
                                         int localPort)
    : io_context_(io_context)
    , socket_(io_context_)
{
}

UDPProtocolAdapter::UDPServer::~UDPServer()
{
    Stop();
}

void UDPProtocolAdapter::UDPServer::Start()
{
    if (isRunning_)
    {
        return;
    }
    
    try
    {
        // 解析地址
        asio::ip::address addr = asio::ip::make_address("0.0.0.0");  // 使用默认地址
        asio::ip::udp::endpoint endpoint(addr, 8888);  // 使用默认端口
        
        // 打开socket
        socket_.open(endpoint.protocol());
        socket_.bind(endpoint);
        
        // 设置广播选项
        if (enableBroadcast_)
        {
            socket_.set_option(asio::socket_base::broadcast(true));
        }
        
        isRunning_ = true;
        
        // 开始接收数据
        StartReceive();
    }
    catch (const std::exception& e)
    {
        if (errorCallback_)
        {
            errorCallback_(std::string("UDP server start failed: ") + e.what(), -1);
        }
    }
}

void UDPProtocolAdapter::UDPServer::Stop()
{
    if (!isRunning_)
    {
        return;
    }
    
    isRunning_ = false;
    
    try
    {
        // 关闭socket
        asio::error_code ec;
        socket_.close(ec);
        
        // 清理端点列表
        std::lock_guard<std::mutex> lock(endpointsMutex_);
        endpoints_.clear();
        clientIdToEndpoint_.clear();
    }
    catch (const std::exception& e)
    {
        if (errorCallback_)
        {
            errorCallback_(std::string("UDP server stop failed: ") + e.what(), -2);
        }
    }
}

bool UDPProtocolAdapter::UDPServer::SendToEndpoint(const asio::ip::udp::endpoint& endpoint,
                                                   const std::vector<uint8_t>& data)
{
    if (!isRunning_ || data.empty())
    {
        return false;
    }
    
    auto buffer = std::make_shared<std::vector<uint8_t>>(data);
    
    socket_.async_send_to(asio::buffer(*buffer), endpoint,
        [this, buffer](const asio::error_code& error, size_t bytes_transferred) {
            HandleSend(error, bytes_transferred, buffer);
        });
    
    return true;
}

bool UDPProtocolAdapter::UDPServer::Broadcast(const std::vector<uint8_t>& data,
                                              const asio::ip::udp::endpoint& broadcastEndpoint)
{
    if (!isRunning_ || !enableBroadcast_ || data.empty())
    {
        return false;
    }
    
    auto buffer = std::make_shared<std::vector<uint8_t>>(data);
    
    socket_.async_send_to(asio::buffer(*buffer), broadcastEndpoint,
        [this, buffer](const asio::error_code& error, size_t bytes_transferred) {
            HandleSend(error, bytes_transferred, buffer);
        });
    
    return true;
}

bool UDPProtocolAdapter::UDPServer::Multicast(const std::vector<uint8_t>& data,
                                              const asio::ip::udp::endpoint& multicastEndpoint)
{
    if (!isRunning_ || multicastGroup_.empty() || data.empty())
    {
        return false;
    }
    
    auto buffer = std::make_shared<std::vector<uint8_t>>(data);
    
    socket_.async_send_to(asio::buffer(*buffer), multicastEndpoint,
        [this, buffer](const asio::error_code& error, size_t bytes_transferred) {
            HandleSend(error, bytes_transferred, buffer);
        });
    
    return true;
}

void UDPProtocolAdapter::UDPServer::SetMessageCallback(
    std::function<void(const ConnectionContext&, const NetworkMessage&)> callback)
{
    messageCallback_ = callback;
}

void UDPProtocolAdapter::UDPServer::SetConnectionCallback(
    std::function<void(const ConnectionContext&, bool)> callback)
{
    connectionCallback_ = callback;
}

void UDPProtocolAdapter::UDPServer::SetErrorCallback(
    std::function<void(const std::string&, int)> callback)
{
    errorCallback_ = callback;
}

std::vector<ConnectionContext> UDPProtocolAdapter::UDPServer::GetConnectedClients() const
{
    std::vector<ConnectionContext> clients;
    
    std::lock_guard<std::mutex> lock(endpointsMutex_);
    auto now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    
    for (const auto& [endpointKey, info] : endpoints_)
    {
        // 只返回最近活跃的端点（30秒内）
        if (now - info.lastActivity < 30000)
        {
            ConnectionContext context;
            context.clientId = 0;  // UDP没有客户端ID概念
            
            // 查找clientId
            for (const auto& [cid, key] : clientIdToEndpoint_)
            {
                if (key == endpointKey)
                {
                    context.clientId = cid;
                    break;
                }
            }
            
            context.clientType = ClientType::STANDARD_CLIENT;
            context.clientName = info.clientName.empty() ? 
                "UDP_Client_" + info.address + ":" + std::to_string(info.port) : 
                info.clientName;
            context.ipAddress = info.address;
            context.port = info.port;
            context.protocolType = "UDP";
            context.isConnected = true;
            context.lastHeartbeatTime = info.lastActivity;
            
            clients.push_back(context);
        }
    }
    
    return clients;
}

size_t UDPProtocolAdapter::UDPServer::GetConnectionCount() const
{
    std::lock_guard<std::mutex> lock(endpointsMutex_);
    auto now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    
    size_t count = 0;
    for (const auto& [endpointKey, info] : endpoints_)
    {
        if (now - info.lastActivity < 30000)  // 30秒内活跃
        {
            count++;
        }
    }
    
    return count;
}

void UDPProtocolAdapter::UDPServer::SetMulticastGroup(const std::string& groupAddress, int ttl)
{
    multicastGroup_ = groupAddress;
    multicastTTL_ = ttl;
    
    if (!multicastGroup_.empty() && socket_.is_open())
    {
        try
        {
            // 加入多播组
            asio::ip::address multicastAddr = asio::ip::make_address(multicastGroup_);
            socket_.set_option(asio::ip::multicast::join_group(multicastAddr));
            
            // 设置TTL
            socket_.set_option(asio::ip::multicast::hops(ttl));
        }
        catch (const std::exception& e)
        {
            if (errorCallback_)
            {
                errorCallback_(std::string("Multicast setup failed: ") + e.what(), -3);
            }
        }
    }
}

void UDPProtocolAdapter::UDPServer::EnableBroadcast(bool enable)
{
    enableBroadcast_ = enable;
    
    if (socket_.is_open())
    {
        try
        {
            socket_.set_option(asio::socket_base::broadcast(enable));
        }
        catch (const std::exception& e)
        {
            if (errorCallback_)
            {
                errorCallback_(std::string("Broadcast setup failed: ") + e.what(), -4);
            }
        }
    }
}

void UDPProtocolAdapter::UDPServer::StartReceive()
{
    if (!isRunning_)
    {
        return;
    }
    
    socket_.async_receive_from(asio::buffer(recv_buffer_), sender_endpoint_,
        [this](const asio::error_code& error, size_t bytes_transferred) {
            HandleReceive(error, bytes_transferred);
        });
}

void UDPProtocolAdapter::UDPServer::HandleReceive(const asio::error_code& error,
                                                  size_t bytes_transferred)
{
    if (error)
    {
        if (error != asio::error::operation_aborted)
        {
            if (errorCallback_)
            {
                errorCallback_(std::string("Receive error: ") + error.message(), error.value());
            }
        }
        return;
    }
    
    if (bytes_transferred > 0)
    {
        // 记录端点信息
        std::string endpointKey = sender_endpoint_.address().to_string() + ":" + 
                                 std::to_string(sender_endpoint_.port());
        
        {
            std::lock_guard<std::mutex> lock(endpointsMutex_);
            
            auto it = endpoints_.find(endpointKey);
            if (it == endpoints_.end())
            {
                // 新端点
                UDPEndpointInfo info;
                info.endpoint = sender_endpoint_;
                info.address = sender_endpoint_.address().to_string();
                info.port = sender_endpoint_.port();
                info.lastActivity = duration_cast<milliseconds>(
                    system_clock::now().time_since_epoch()).count();
                info.clientName = "UDP_Client_" + std::to_string(endpoints_.size() + 1);
                
                endpoints_[endpointKey] = info;
                
                // 分配clientId
                ClientId clientId = static_cast<ClientId>(endpoints_.size());
                clientIdToEndpoint_[clientId] = endpointKey;
                
                // 通知新端点
                if (connectionCallback_)
                {
                    ConnectionContext context;
                    context.clientId = clientId;
                    context.clientType = ClientType::STANDARD_CLIENT;
                    context.clientName = info.clientName;
                    context.ipAddress = info.address;
                    context.port = info.port;
                    context.protocolType = "UDP";
                    context.isConnected = true;
                    context.lastHeartbeatTime = info.lastActivity;
                    
                    connectionCallback_(context, true);
                }
            }
            else
            {
                // 更新现有端点的活动时间
                it->second.lastActivity = duration_cast<milliseconds>(
                    system_clock::now().time_since_epoch()).count();
            }
        }
        
        // 处理接收到的数据
        std::vector<uint8_t> data(recv_buffer_.begin(), recv_buffer_.begin() + bytes_transferred);
        
        // 反序列化为NetworkMessage
        NetworkMessage msg;
        msg.payload = data;
        msg.type = MessageType::DATA;
        msg.timestamp = duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()).count();
        
        // 获取clientId
        ClientId clientId = 0;
        {
            std::lock_guard<std::mutex> lock(endpointsMutex_);
            for (const auto& [cid, key] : clientIdToEndpoint_)
            {
                if (key == endpointKey)
                {
                    clientId = cid;
                    break;
                }
            }
        }
        msg.sourceClientId = clientId;
        
        // 调用消息回调
        if (messageCallback_)
        {
            ConnectionContext context;
            context.clientId = clientId;
            context.clientType = ClientType::STANDARD_CLIENT;
            context.ipAddress = sender_endpoint_.address().to_string();
            context.port = sender_endpoint_.port();
            context.protocolType = "UDP";
            context.isConnected = true;
            context.lastHeartbeatTime = duration_cast<milliseconds>(
                system_clock::now().time_since_epoch()).count();
            
            // 查找客户端名称
            {
                std::lock_guard<std::mutex> lock(endpointsMutex_);
                auto it = endpoints_.find(endpointKey);
                if (it != endpoints_.end())
                {
                    context.clientName = it->second.clientName;
                }
            }
            
            messageCallback_(context, msg);
        }
    }
    
    // 继续接收
    if (isRunning_)
    {
        StartReceive();
    }
}

void UDPProtocolAdapter::UDPServer::HandleSend(const asio::error_code& error,
                                               size_t bytes_transferred,
                                               std::shared_ptr<std::vector<uint8_t>> buffer)
{
    if (error)
    {
        if (errorCallback_)
        {
            errorCallback_(std::string("Send error: ") + error.message(), error.value());
        }
    }
}

#endif // USE_ASIO

} // namespace CommunicationModule