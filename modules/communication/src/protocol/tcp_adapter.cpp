#include "protocol/tcp_adapter.h"

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>

#ifdef USE_ASIO
#include <asio/read.hpp>
#include <asio/write.hpp>
#include <asio/connect.hpp>
#endif

using namespace std::chrono;

namespace CommunicationModule
{

TCPProtocolAdapter::TCPProtocolAdapter()
    : BaseProtocolAdapter("TCP", "TCPProtocolAdapter")
    , listenAddress_("0.0.0.0")
    , listenPort_(8080)
    , numIOThreads_(2)
    , enableKeepAlive_(true)
    , keepAliveIdle_(60)
    , keepAliveInterval_(5)
    , keepAliveCount_(3)
    , enableNoDelay_(true)
    , receiveBufferSize_(8192)
    , sendBufferSize_(8192)
{
}

TCPProtocolAdapter::~TCPProtocolAdapter()
{
    DoStop();
    DoCleanup();
}

bool TCPProtocolAdapter::DoInitialize()
{
#ifdef USE_ASIO
    // 解析配置参数
    auto addressIt = GetConfig().protocolParams.find("listen_address");
    if (addressIt != GetConfig().protocolParams.end())
    {
        listenAddress_ = addressIt->second;
    }
    else if (!GetConfig().serverAddress.empty())
    {
        listenAddress_ = GetConfig().serverAddress;
    }

    auto portIt = GetConfig().protocolParams.find("listen_port");
    if (portIt != GetConfig().protocolParams.end())
    {
        listenPort_ = std::stoi(portIt->second);
    }
    else if (GetConfig().serverPort > 0)
    {
        listenPort_ = GetConfig().serverPort;
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

    auto keepAliveIt = GetConfig().protocolParams.find("keep_alive");
    if (keepAliveIt != GetConfig().protocolParams.end())
    {
        enableKeepAlive_ = (keepAliveIt->second == "true" || keepAliveIt->second == "1");
    }

    auto noDelayIt = GetConfig().protocolParams.find("tcp_nodelay");
    if (noDelayIt != GetConfig().protocolParams.end())
    {
        enableNoDelay_ = (noDelayIt->second == "true" || noDelayIt->second == "1");
    }

    // 创建IO上下文
    io_context_ = std::make_shared<asio::io_context>();
    work_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
        asio::make_work_guard(*io_context_));  // 修改为正确的work_guard创建方式

    // 创建TCP服务器
    tcpServer_ = std::make_unique<TCPServer>(*io_context_, listenAddress_, listenPort_);

    // 设置回调
    tcpServer_->SetMessageCallback([this](const ConnectionContext& context, 
                                          const NetworkMessage& message) {
        NotifyMessageReceived(context, message);
        messagesReceived_.fetch_add(1);
        UpdateStatistic("messages_received", 1);
        UpdateStatistic("bytes_received", message.payload.size());
    });

    tcpServer_->SetConnectionCallback([this](const ConnectionContext& context, bool connected) {
        NotifyConnectionChanged(context, connected);
        if (connected)
        {
            connectionsAccepted_.fetch_add(1);
            UpdateStatistic("connections", 1);
        }
        else
        {
            UpdateStatistic("disconnections", 1);
        }
    });

    tcpServer_->SetErrorCallback([this](const std::string& error, int code) {
        NotifyError(error, code);
    });

    return true;
#else
    NotifyError("ASIO support not compiled in", -1);
    return false;
#endif
}

bool TCPProtocolAdapter::DoStart()
{
#ifdef USE_ASIO
    if (shouldStop_ || !io_context_ || !tcpServer_)
    {
        return false;
    }

    // 启动IO线程
    try
    {
        for (int i = 0; i < numIOThreads_; ++i)
        {
            io_threads_.emplace_back(&TCPProtocolAdapter::IOContextThreadFunc, this);
        }
    }
    catch (const std::exception& e)
    {
        NotifyError(std::string("Failed to start IO threads: ") + e.what(), -2);
        return false;
    }

    // 启动TCP服务器
    tcpServer_->Start();

    // 启动维护线程
    try
    {
        maintenanceThread_ = std::thread(&TCPProtocolAdapter::MaintenanceThreadFunc, this);
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

void TCPProtocolAdapter::DoStop()
{
    shouldStop_ = true;

#ifdef USE_ASIO
    // 停止TCP服务器
    if (tcpServer_)
    {
        tcpServer_->Stop();
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

void TCPProtocolAdapter::DoCleanup()
{
#ifdef USE_ASIO
    tcpServer_.reset();
    work_.reset();
    io_context_.reset();
#endif
}

bool TCPProtocolAdapter::SendToClient(ClientId clientId, const NetworkMessage& message)
{
#ifdef USE_ASIO
    if (!IsRunning() || !tcpServer_)
    {
        return false;
    }

    auto data = SerializeTCPMessage(message);
    bool success = tcpServer_->SendToClient(clientId, data);

    if (success)
    {
        messagesSent_.fetch_add(1);
        UpdateStatistic("messages_sent", 1);
        UpdateStatistic("bytes_sent", data.size());
    }

    return success;
#else
    return false;
#endif
}

int TCPProtocolAdapter::Broadcast(const NetworkMessage& message)
{
#ifdef USE_ASIO
    if (!IsRunning() || !tcpServer_)
    {
        return 0;
    }

    auto data = SerializeTCPMessage(message);
    tcpServer_->Broadcast(data);

    int clientCount = static_cast<int>(tcpServer_->GetConnectionCount());
    
    messagesSent_.fetch_add(clientCount);
    UpdateStatistic("messages_sent", clientCount);
    UpdateStatistic("bytes_sent", data.size() * clientCount);

    return clientCount;
#else
    return 0;
#endif
}

int TCPProtocolAdapter::PublishToTopic(const std::string& topic, const NetworkMessage& message)
{
    // TCP协议本身不支持主题，这里通过广播实现
    NetworkMessage msgWithTopic = message;
    msgWithTopic.topic = topic;
    msgWithTopic.headers["topic"] = topic;
    
    return Broadcast(msgWithTopic);
}

std::vector<ConnectionContext> TCPProtocolAdapter::GetConnectedClients() const
{
#ifdef USE_ASIO
    if (tcpServer_)
    {
        return tcpServer_->GetConnectedClients();
    }
#endif
    return {};
}

bool TCPProtocolAdapter::DisconnectClient(ClientId clientId, const std::string& reason)
{
#ifdef USE_ASIO
    if (tcpServer_)
    {
        tcpServer_->DisconnectClient(clientId, reason);
        return true;
    }
#endif
    return false;
}

size_t TCPProtocolAdapter::GetConnectionCount() const
{
#ifdef USE_ASIO
    if (tcpServer_)
    {
        return tcpServer_->GetConnectionCount();
    }
#endif
    return 0;
}

size_t TCPProtocolAdapter::GetActiveConnectionCount() const
{
    // TCP连接都是活跃的（除非已断开）
    return GetConnectionCount();
}

bool TCPProtocolAdapter::SendRawData(ClientId clientId, const std::vector<uint8_t>& data)
{
#ifdef USE_ASIO
    if (!IsRunning() || !tcpServer_)
    {
        return false;
    }

    return tcpServer_->SendToClient(clientId, data);
#else
    return false;
#endif
}

void TCPProtocolAdapter::SetKeepAlive(bool enable, int idleTime, int interval, int count)
{
    enableKeepAlive_ = enable;
    keepAliveIdle_ = idleTime;
    keepAliveInterval_ = interval;
    keepAliveCount_ = count;
}

void TCPProtocolAdapter::SetNoDelay(bool enable)
{
    enableNoDelay_ = enable;
}

void TCPProtocolAdapter::SetReceiveBufferSize(size_t size)
{
    receiveBufferSize_ = size;
}

void TCPProtocolAdapter::SetSendBufferSize(size_t size)
{
    sendBufferSize_ = size;
}

std::vector<uint8_t> TCPProtocolAdapter::SerializeTCPMessage(const NetworkMessage& msg) const
{
    // 简单的消息格式: [消息头长度(4字节)][消息头JSON][有效载荷]
    
    // 构建消息头
    std::map<std::string, std::string> headers = msg.headers;
    headers["message_type"] = std::to_string(static_cast<int>(msg.type));
    headers["source_client"] = std::to_string(msg.sourceClientId);
    headers["target_client"] = std::to_string(msg.targetClientId);
    headers["timestamp"] = std::to_string(msg.timestamp);
    
    if (!msg.topic.empty())
    {
        headers["topic"] = msg.topic;
    }
    
    // 将消息头转换为JSON（简化实现）
    std::string headerJson = "{";
    bool first = true;
    for (const auto& [key, value] : headers)
    {
        if (!first) headerJson += ",";
        headerJson += "\"" + key + "\":\"" + value + "\"";
        first = false;
    }
    headerJson += "}";
    
    // 构建完整消息
    uint32_t headerSize = static_cast<uint32_t>(headerJson.size());
    std::vector<uint8_t> buffer(sizeof(headerSize) + headerSize + msg.payload.size());
    
    // 写入消息头长度
    uint8_t* ptr = buffer.data();
    std::memcpy(ptr, &headerSize, sizeof(headerSize));
    ptr += sizeof(headerSize);
    
    // 写入消息头
    std::memcpy(ptr, headerJson.data(), headerSize);
    ptr += headerSize;
    
    // 写入有效载荷
    if (!msg.payload.empty())
    {
        std::memcpy(ptr, msg.payload.data(), msg.payload.size());
    }
    
    return buffer;
}

NetworkMessage TCPProtocolAdapter::DeserializeTCPMessage(const std::vector<uint8_t>& data) const
{
    NetworkMessage msg;
    
    if (data.size() < sizeof(uint32_t))
    {
        return msg;
    }
    
    // 读取消息头长度
    uint32_t headerSize = 0;
    std::memcpy(&headerSize, data.data(), sizeof(headerSize));
    
    if (data.size() < sizeof(headerSize) + headerSize)
    {
        return msg;
    }
    
    // 解析消息头（简化实现，实际应该使用JSON解析器）
    const char* headerJson = reinterpret_cast<const char*>(data.data() + sizeof(headerSize));
    std::string headerStr(headerJson, headerSize);
    
    // 简单解析JSON（实际项目应使用rapidjson等库）
    // 这里仅作演示
    size_t pos = headerStr.find("\"message_type\":\"");
    if (pos != std::string::npos)
    {
        pos += 16;
        size_t end = headerStr.find("\"", pos);
        if (end != std::string::npos)
        {
            int type = std::stoi(headerStr.substr(pos, end - pos));
            msg.type = static_cast<MessageType>(type);
        }
    }
    
    // 提取有效载荷
    size_t payloadOffset = sizeof(headerSize) + headerSize;
    if (data.size() > payloadOffset)
    {
        msg.payload.assign(data.begin() + payloadOffset, data.end());
    }
    
    msg.timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    
    return msg;
}

void TCPProtocolAdapter::IOContextThreadFunc()
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

void TCPProtocolAdapter::MaintenanceThreadFunc()
{
    while (!shouldStop_)
    {
        // 定期更新统计信息
        UpdateStatistic("connections_active", GetConnectionCount());
        
        // 清理不活跃连接（由TCPServer内部处理）
        
        std::this_thread::sleep_for(milliseconds(1000));
    }
}

#ifdef USE_ASIO
// =============== TCPConnection 实现 ===============

TCPProtocolAdapter::TCPConnection::TCPConnection(asio::io_context& io_context,
                                                 const std::string& adapterName,
                                                 ClientId clientId)
    : socket_(io_context)
    , adapterName_(adapterName)
    , clientId_(clientId)
    , remotePort_(0)
    , isConnected_(false)
    , lastActivity_(steady_clock::now())
{
}

void TCPProtocolAdapter::TCPConnection::Start()
{
    if (isConnected_)
    {
        return;
    }
    
    try
    {
        // 获取远程端点信息
        asio::ip::tcp::endpoint remoteEndpoint = socket_.remote_endpoint();
        remoteAddress_ = remoteEndpoint.address().to_string();
        remotePort_ = remoteEndpoint.port();
        
        // 设置TCP选项
        socket_.set_option(asio::ip::tcp::no_delay(true));
        socket_.set_option(asio::socket_base::keep_alive(true));
        
        // 创建连接上下文
        ConnectionContext context = GetContext();
        
        // 通知连接建立
        if (connectionCallback_)
        {
            connectionCallback_(context, true);
        }
        
        isConnected_ = true;
        lastActivity_ = steady_clock::now();
        
        // 开始读取数据
        DoRead();
    }
    catch (const std::exception& e)
    {
        if (errorCallback_)
        {
            errorCallback_(std::string("Connection start failed: ") + e.what(), -1);
        }
    }
}

void TCPProtocolAdapter::TCPConnection::Stop()
{
    if (!isConnected_)
    {
        return;
    }
    
    isConnected_ = false;
    
    try
    {
        // 关闭socket
        asio::error_code ec;
        socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        socket_.close(ec);
        
        // 通知连接断开
        if (connectionCallback_)
        {
            ConnectionContext context = GetContext();
            connectionCallback_(context, false);
        }
    }
    catch (const std::exception& e)
    {
        if (errorCallback_)
        {
            errorCallback_(std::string("Connection stop failed: ") + e.what(), -2);
        }
    }
}

void TCPProtocolAdapter::TCPConnection::Send(const std::vector<uint8_t>& data)
{
    if (!isConnected_ || data.empty())
    {
        return;
    }
    
    std::lock_guard<std::mutex> lock(writeMutex_);
    
    // 添加到写队列
    writeQueue_.push(data);
    
    // 如果当前没有正在进行的写操作，开始写
    if (writeQueue_.size() == 1)
    {
        DoWrite();
    }
}

void TCPProtocolAdapter::TCPConnection::SetMessageCallback(
    std::function<void(const ConnectionContext&, const NetworkMessage&)> callback)
{
    messageCallback_ = callback;
}

void TCPProtocolAdapter::TCPConnection::SetConnectionCallback(
    std::function<void(const ConnectionContext&, bool)> callback)
{
    connectionCallback_ = callback;
}

void TCPProtocolAdapter::TCPConnection::SetErrorCallback(
    std::function<void(const std::string&, int)> callback)
{
    errorCallback_ = callback;
}

ConnectionContext TCPProtocolAdapter::TCPConnection::GetContext() const
{
    ConnectionContext context;
    context.clientId = clientId_;
    context.clientType = ClientType::STANDARD_CLIENT;
    context.clientName = adapterName_ + "_Client_" + std::to_string(clientId_);
    context.ipAddress = remoteAddress_;
    context.port = remotePort_;
    context.protocolType = "TCP";
    context.isConnected = isConnected_;
    context.lastHeartbeatTime = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    
    return context;
}

void TCPProtocolAdapter::TCPConnection::DoRead()
{
    if (!isConnected_)
    {
        return;
    }
    
    auto self = shared_from_this();
    
    socket_.async_read_some(asio::buffer(buffer_),
        [this, self](const asio::error_code& error, size_t bytes_transferred) {
            HandleRead(error, bytes_transferred);
        });
}

void TCPProtocolAdapter::TCPConnection::DoWrite()
{
    if (!isConnected_ || writeQueue_.empty())
    {
        return;
    }
    
    std::lock_guard<std::mutex> lock(writeMutex_);
    
    if (writeQueue_.empty())
    {
        return;
    }
    
    // 获取要发送的数据
    writeBuffer_ = writeQueue_.front();
    writeQueue_.pop();
    
    auto self = shared_from_this();
    
    asio::async_write(socket_, asio::buffer(writeBuffer_),
        [this, self](const asio::error_code& error, size_t bytes_transferred) {
            HandleWrite(error, bytes_transferred);
        });
}

void TCPProtocolAdapter::TCPConnection::HandleRead(const asio::error_code& error,
                                                   size_t bytes_transferred)
{
    if (error)
    {
        if (error == asio::error::eof || error == asio::error::connection_reset)
        {
            // 连接正常关闭
            Stop();
        }
        else if (error != asio::error::operation_aborted)
        {
            // 其他错误
            if (errorCallback_)
            {
                errorCallback_(std::string("Read error: ") + error.message(), error.value());
            }
            Stop();
        }
        return;
    }
    
    if (bytes_transferred > 0)
    {
        // 更新活动时间
        UpdateLastActivity();
        
        // 处理接收到的数据
        std::vector<uint8_t> data(buffer_.begin(), buffer_.begin() + bytes_transferred);
        
        // 这里应该实现更完整的消息解析
        // 简化处理：直接反序列化为NetworkMessage
        NetworkMessage msg;
        msg.payload = data;
        msg.type = MessageType::DATA;
        msg.timestamp = duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()).count();
        msg.sourceClientId = clientId_;
        
        // 调用消息回调
        if (messageCallback_)
        {
            ConnectionContext context = GetContext();
            messageCallback_(context, msg);
        }
    }
    
    // 继续读取
    if (isConnected_)
    {
        DoRead();
    }
}

void TCPProtocolAdapter::TCPConnection::HandleWrite(const asio::error_code& error,
                                                    size_t bytes_transferred)
{
    if (error)
    {
        if (errorCallback_)
        {
            errorCallback_(std::string("Write error: ") + error.message(), error.value());
        }
        Stop();
        return;
    }
    
    // 更新活动时间
    UpdateLastActivity();
    
    // 继续发送队列中的下一条消息
    std::lock_guard<std::mutex> lock(writeMutex_);
    if (!writeQueue_.empty())
    {
        DoWrite();
    }
}

// =============== TCPServer 实现 ===============

TCPProtocolAdapter::TCPServer::TCPServer(asio::io_context& io_context,
                                         const std::string& address,
                                         int port)
    : io_context_(io_context)
    , acceptor_(io_context_)
    , listenAddress_(address)
    , listenPort_(port)
{
}

TCPProtocolAdapter::TCPServer::~TCPServer()
{
    Stop();
}

void TCPProtocolAdapter::TCPServer::Start()
{
    if (isRunning_)
    {
        return;
    }
    
    try
    {
        // 解析地址
        asio::ip::address addr = asio::ip::make_address(listenAddress_);
        asio::ip::tcp::endpoint endpoint(addr, listenPort_);
        
        // 打开acceptor
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();
        
        isRunning_ = true;
        
        // 开始接受连接
        StartAccept();
    }
    catch (const std::exception& e)
    {
        if (errorCallback_)
        {
            errorCallback_(std::string("TCP server start failed: ") + e.what(), -1);
        }
    }
}

void TCPProtocolAdapter::TCPServer::Stop()
{
    if (!isRunning_)
    {
        return;
    }
    
    isRunning_ = false;
    
    try
    {
        // 关闭acceptor
        asio::error_code ec;
        acceptor_.close(ec);
        
        // 断开所有连接
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        for (auto& [clientId, connection] : connections_)
        {
            connection->Stop();
        }
        connections_.clear();
    }
    catch (const std::exception& e)
    {
        if (errorCallback_)
        {
            errorCallback_(std::string("TCP server stop failed: ") + e.what(), -2);
        }
    }
}

void TCPProtocolAdapter::TCPServer::Broadcast(const std::vector<uint8_t>& data)
{
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    for (auto& [clientId, connection] : connections_)
    {
        if (connection->IsConnected())
        {
            connection->Send(data);
        }
    }
}

bool TCPProtocolAdapter::TCPServer::SendToClient(ClientId clientId, const std::vector<uint8_t>& data)
{
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    auto it = connections_.find(clientId);
    if (it != connections_.end() && it->second->IsConnected())
    {
        it->second->Send(data);
        return true;
    }
    
    return false;
}

void TCPProtocolAdapter::TCPServer::DisconnectClient(ClientId clientId, const std::string& reason)
{
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    auto it = connections_.find(clientId);
    if (it != connections_.end())
    {
        it->second->Stop();
        connections_.erase(it);
    }
}

void TCPProtocolAdapter::TCPServer::SetMessageCallback(
    std::function<void(const ConnectionContext&, const NetworkMessage&)> callback)
{
    messageCallback_ = callback;
}

void TCPProtocolAdapter::TCPServer::SetConnectionCallback(
    std::function<void(const ConnectionContext&, bool)> callback)
{
    connectionCallback_ = callback;
}

void TCPProtocolAdapter::TCPServer::SetErrorCallback(
    std::function<void(const std::string&, int)> callback)
{
    errorCallback_ = callback;
}

std::vector<ConnectionContext> TCPProtocolAdapter::TCPServer::GetConnectedClients() const
{
    std::vector<ConnectionContext> clients;
    
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    clients.reserve(connections_.size());
    
    for (const auto& [clientId, connection] : connections_)
    {
        if (connection->IsConnected())
        {
            clients.push_back(connection->GetContext());
        }
    }
    
    return clients;
}

size_t TCPProtocolAdapter::TCPServer::GetConnectionCount() const
{
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    size_t count = 0;
    for (const auto& [clientId, connection] : connections_)
    {
        if (connection->IsConnected())
        {
            count++;
        }
    }
    
    return count;
}

void TCPProtocolAdapter::TCPServer::StartAccept()
{
    if (!isRunning_)
    {
        return;
    }
    
    ClientId newClientId = nextClientId_++;
    
    auto new_connection = TCPConnection::Create(io_context_, "TCPAdapter", newClientId);
    
    // 设置回调
    new_connection->SetMessageCallback(messageCallback_);
    new_connection->SetConnectionCallback(connectionCallback_);
    new_connection->SetErrorCallback(errorCallback_);
    
    acceptor_.async_accept(new_connection->Socket(),
        [this, new_connection](const asio::error_code& error) {
            HandleAccept(new_connection, error);
        });
}

void TCPProtocolAdapter::TCPServer::HandleAccept(TCPConnection::Pointer new_connection,
                                                 const asio::error_code& error)
{
    if (!error)
    {
        // 保存连接
        {
            std::lock_guard<std::mutex> lock(connectionsMutex_);
            connections_[new_connection->GetClientId()] = new_connection;
        }
        
        // 启动连接
        new_connection->Start();
        
        // 继续接受新连接
        StartAccept();
    }
    else if (error != asio::error::operation_aborted)
    {
        if (errorCallback_)
        {
            errorCallback_(std::string("Accept error: ") + error.message(), error.value());
        }
    }
}

#endif // USE_ASIO

} // namespace CommunicationModule