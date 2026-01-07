/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        udp_adapter.h
Version:     1.0
Author:      cjx
start date:
Description: UDP协议适配器实现
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-1-05      cjx            create
*****************************************************************/

#ifndef UDP_PROTOCOL_ADAPTER_H
#define UDP_PROTOCOL_ADAPTER_H

#include "protocol/base_adapter.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef USE_ASIO
#include <asio.hpp>
#endif

namespace CommunicationModule
{

/**
 * @brief UDP协议适配器
 * 基于ASIO实现的UDP协议适配器
 */
class UDPProtocolAdapter : public BaseProtocolAdapter
{
public:
    UDPProtocolAdapter();
    virtual ~UDPProtocolAdapter();

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
    void SetMulticastGroup(const std::string &groupAddress, int ttl = 1);
    void EnableBroadcast(bool enable);
    void SetReceiveBufferSize(size_t size);
    void SetSendBufferSize(size_t size);

protected:
    bool DoInitialize() override;
    bool DoStart() override;
    void DoStop() override;
    void DoCleanup() override;

private:
#ifdef USE_ASIO
    // UDP端点信息
    struct UDPEndpointInfo
    {
        asio::ip::udp::endpoint endpoint;
        std::string address;
        int port;
        int64_t lastActivity;
        std::unordered_set<std::string> subscribedTopics;
        std::string clientName;
    };

    // UDP服务器类
    class UDPServer
    {
    public:
        UDPServer(asio::io_context &io_context,
                  const std::string &localAddress,
                  int localPort);
        ~UDPServer();

        void Start();
        void Stop();
        bool SendToEndpoint(const asio::ip::udp::endpoint &endpoint,
                           const std::vector<uint8_t> &data);
        bool Broadcast(const std::vector<uint8_t> &data,
                      const asio::ip::udp::endpoint &broadcastEndpoint);
        bool Multicast(const std::vector<uint8_t> &data,
                      const asio::ip::udp::endpoint &multicastEndpoint);

        void SetMessageCallback(std::function<void(const ConnectionContext &,
                                                   const NetworkMessage &)> callback);
        void SetConnectionCallback(std::function<void(const ConnectionContext &,
                                                      bool)> callback);
        void SetErrorCallback(std::function<void(const std::string &, int)> callback);

        std::vector<ConnectionContext> GetConnectedClients() const;
        size_t GetConnectionCount() const;

        void SetMulticastGroup(const std::string &groupAddress, int ttl);
        void EnableBroadcast(bool enable);

    private:
        void StartReceive();
        void HandleReceive(const asio::error_code &error,
                          size_t bytes_transferred);
        void HandleSend(const asio::error_code &error,
                       size_t bytes_transferred,
                       std::shared_ptr<std::vector<uint8_t>> buffer);

        asio::io_context &io_context_;
        asio::ip::udp::socket socket_;
        asio::ip::udp::endpoint sender_endpoint_;
        std::array<uint8_t, 65507> recv_buffer_; // UDP最大包大小

        mutable std::mutex endpointsMutex_;
        std::unordered_map<std::string, UDPEndpointInfo> endpoints_; // key: address:port
        std::unordered_map<ClientId, std::string> clientIdToEndpoint_;

        std::function<void(const ConnectionContext &, const NetworkMessage &)> messageCallback_;
        std::function<void(const ConnectionContext &, bool)> connectionCallback_;
        std::function<void(const std::string &, int)> errorCallback_;

        std::atomic<bool> isRunning_{false};
        std::string multicastGroup_;
        int multicastTTL_;
        bool enableBroadcast_;
    };
#endif

    // 序列化/反序列化方法
    std::vector<uint8_t> SerializeUDPMessage(const NetworkMessage &msg) const;
    NetworkMessage DeserializeUDPMessage(const std::vector<uint8_t> &data) const;

    // 线程函数
    void IOContextThreadFunc();
    void MaintenanceThreadFunc();

#ifdef USE_ASIO
    // ASIO相关
    std::shared_ptr<asio::io_context> io_context_;
    std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_;
    std::unique_ptr<UDPServer> udpServer_;
    std::vector<std::thread> io_threads_;
#endif

    // 配置
    std::string localAddress_;
    int localPort_;
    int numIOThreads_;
    std::string multicastGroup_;
    int multicastTTL_;
    bool enableBroadcast_;
    size_t receiveBufferSize_;
    size_t sendBufferSize_;

    // 线程控制
    std::atomic<bool> shouldStop_{false};
    std::thread maintenanceThread_;

    // 统计
    mutable std::atomic<uint64_t> packetsSent_{0};
    mutable std::atomic<uint64_t> packetsReceived_{0};
    mutable std::atomic<uint64_t> bytesSent_{0};
    mutable std::atomic<uint64_t> bytesReceived_{0};
};

} // namespace CommunicationModule

#endif // UDP_PROTOCOL_ADAPTER_H