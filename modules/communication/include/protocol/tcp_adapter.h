/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        tcp_adapter.h
Version:     1.0
Author:      cjx
start date:
Description: TCP协议适配器实现
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-1-05      cjx            create
*****************************************************************/

#ifndef TCP_PROTOCOL_ADAPTER_H
#define TCP_PROTOCOL_ADAPTER_H

#include "protocol/base_adapter.h"

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef USE_ASIO
#include <asio.hpp>
#endif

namespace CommunicationModule
{

/**
 * @brief TCP协议适配器
 * 基于ASIO实现的TCP协议适配器
 */
class TCPProtocolAdapter : public BaseProtocolAdapter
{
public:
    TCPProtocolAdapter();
    virtual ~TCPProtocolAdapter();

    // IProtocolAdapter 接口实现
    bool SendToClient(ClientId clientId, const NetworkMessage &message) override;
    int Broadcast(const NetworkMessage &message) override;
    int PublishToTopic(const std::string &topic, const NetworkMessage &message) override;

    std::vector<ConnectionContext> GetConnectedClients() const override;
    bool DisconnectClient(ClientId clientId, const std::string &reason = "") override;

    size_t GetConnectionCount() const override;
    size_t GetActiveConnectionCount() const override;

    bool SendRawData(ClientId clientId, const std::vector<uint8_t> &data) override;

    // 扩展功能
    void SetKeepAlive(bool enable, int idleTime = 60, int interval = 5, int count = 3);
    void SetNoDelay(bool enable);
    void SetReceiveBufferSize(size_t size);
    void SetSendBufferSize(size_t size);

protected:
    bool DoInitialize() override;
    bool DoStart() override;
    void DoStop() override;
    void DoCleanup() override;

private:
#ifdef USE_ASIO
    // TCP连接类
    class TCPConnection : public std::enable_shared_from_this<TCPConnection>
    {
    public:
        using Pointer = std::shared_ptr<TCPConnection>;

        static Pointer Create(asio::io_context &io_context,
                              const std::string &adapterName,
                              ClientId clientId)
        {
            return Pointer(new TCPConnection(io_context, adapterName, clientId));
        }

        asio::ip::tcp::socket &Socket() { return socket_; }
        ClientId GetClientId() const { return clientId_; }
        std::string GetRemoteAddress() const { return remoteAddress_; }
        int GetRemotePort() const { return remotePort_; }

        void Start();
        void Stop();
        void Send(const std::vector<uint8_t> &data);
        bool IsConnected() const { return isConnected_; }
        void UpdateLastActivity() { lastActivity_ = std::chrono::steady_clock::now(); }
        std::chrono::steady_clock::time_point GetLastActivity() const { return lastActivity_; }

        void SetMessageCallback(std::function<void(const ConnectionContext &, const NetworkMessage &)> callback);
        void SetConnectionCallback(std::function<void(const ConnectionContext &, bool)> callback);
        void SetErrorCallback(std::function<void(const std::string &, int)> callback);

        ConnectionContext GetContext() const;

    private:
        TCPConnection(asio::io_context &io_context,
                      const std::string &adapterName,
                      ClientId clientId);

        void DoRead();
        void DoWrite();
        void HandleRead(const asio::error_code &error, size_t bytes_transferred);
        void HandleWrite(const asio::error_code &error, size_t bytes_transferred);

        asio::ip::tcp::socket socket_;
        std::string adapterName_;
        ClientId clientId_;
        std::string remoteAddress_;
        int remotePort_;

        std::array<uint8_t, 8192> buffer_;
        std::vector<uint8_t> writeBuffer_;
        std::mutex writeMutex_;
        std::queue<std::vector<uint8_t>> writeQueue_;

        std::atomic<bool> isConnected_{false};
        std::chrono::steady_clock::time_point lastActivity_;

        std::function<void(const ConnectionContext &, const NetworkMessage &)> messageCallback_;
        std::function<void(const ConnectionContext &, bool)> connectionCallback_;
        std::function<void(const std::string &, int)> errorCallback_;
    };

    // TCP服务器类
    class TCPServer
    {
    public:
        TCPServer(asio::io_context &io_context, const std::string &address, int port);
        ~TCPServer();

        void Start();
        void Stop();
        void Broadcast(const std::vector<uint8_t> &data);
        bool SendToClient(ClientId clientId, const std::vector<uint8_t> &data);
        void DisconnectClient(ClientId clientId, const std::string &reason);

        void SetMessageCallback(std::function<void(const ConnectionContext &, const NetworkMessage &)> callback);
        void SetConnectionCallback(std::function<void(const ConnectionContext &, bool)> callback);
        void SetErrorCallback(std::function<void(const std::string &, int)> callback);

        std::vector<ConnectionContext> GetConnectedClients() const;
        size_t GetConnectionCount() const;

    private:
        void StartAccept();
        void HandleAccept(TCPConnection::Pointer new_connection,
                          const asio::error_code &error);

        asio::io_context &io_context_;
        asio::ip::tcp::acceptor acceptor_;
        std::string listenAddress_;
        int listenPort_;

        mutable std::mutex connectionsMutex_;
        std::unordered_map<ClientId, TCPConnection::Pointer> connections_;
        std::atomic<ClientId> nextClientId_{1};

        std::function<void(const ConnectionContext &, const NetworkMessage &)> messageCallback_;
        std::function<void(const ConnectionContext &, bool)> connectionCallback_;
        std::function<void(const std::string &, int)> errorCallback_;

        std::atomic<bool> isRunning_{false};
    };
#endif

    // 序列化/反序列化方法
    std::vector<uint8_t> SerializeTCPMessage(const NetworkMessage &msg) const;
    NetworkMessage DeserializeTCPMessage(const std::vector<uint8_t> &data) const;

    // 线程函数
    void IOContextThreadFunc();
    void MaintenanceThreadFunc();

#ifdef USE_ASIO
    // ASIO相关
    std::shared_ptr<asio::io_context> io_context_;
    std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_;  // 修改为executor_work_guard
    std::unique_ptr<TCPServer> tcpServer_;
    std::vector<std::thread> io_threads_;
#endif

    // 配置
    std::string listenAddress_;
    int listenPort_;
    int numIOThreads_;
    bool enableKeepAlive_;
    int keepAliveIdle_;
    int keepAliveInterval_;
    int keepAliveCount_;
    bool enableNoDelay_;
    size_t receiveBufferSize_;
    size_t sendBufferSize_;

    // 线程控制
    std::atomic<bool> shouldStop_{false};
    std::thread maintenanceThread_;

    // 统计
    mutable std::atomic<uint64_t> messagesSent_{0};
    mutable std::atomic<uint64_t> messagesReceived_{0};
    mutable std::atomic<uint64_t> connectionsAccepted_{0};
};

} // namespace CommunicationModule

#endif // TCP_PROTOCOL_ADAPTER_H