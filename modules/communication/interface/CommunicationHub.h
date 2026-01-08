/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        CommunicationHub.h
Version:     1.0
Author:      cjx
start date:
Description: 通用通信枢纽接口
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-1-04      cjx            create

*****************************************************************/

#ifndef COMMUNICATION_HUB_INTERFACE_H
#define COMMUNICATION_HUB_INTERFACE_H

#include <any>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace CommunicationModule
{
// --- 通用类型定义 ---
using ClientId = uint64_t;

// 通用的消息类型，用户可以扩展
enum class MessageType
{
    CONTROL = 0, // 控制消息
    DATA,        // 数据消息
    COMMAND,     // 命令消息
    STATUS,      // 状态消息
    EVENT,       // 事件消息
    HEARTBEAT,   // 心跳消息
    CUSTOM       // 自定义消息类型
};

// 客户端类型，用户可扩展
enum class ClientType
{
    STANDARD_CLIENT, // 标准客户端
    ADMIN_CLIENT,    // 管理客户端
    INTERNAL_MODULE, // 内部模块
    EXTERNAL_SYSTEM, // 外部系统
    UNKNOWN
};

/**
 * @brief 通用网络消息结构
 */
struct NetworkMessage
{
    MessageType type;                           // 消息类型
    std::vector<uint8_t> payload;               // 消息体（序列化数据）
    ClientId sourceClientId = 0;                // 源客户端ID
    ClientId targetClientId = 0;                // 目标客户端ID（0表示广播）
    std::string topic;                          // 发布/订阅主题
    int64_t timestamp = 0;                      // 时间戳（毫秒）
    std::map<std::string, std::any> metadata;   // 消息元数据（支持多种类型）
    std::map<std::string, std::string> headers; // 消息头（字符串类型）

    // 便捷方法
    bool IsBroadcast() const { return targetClientId == 0; }
    bool HasTopic() const { return !topic.empty(); }
    std::string GetString(const std::string &key, const std::string &defaultValue = "") const
    {
        auto it = headers.find(key);
        return it != headers.end() ? it->second : defaultValue;
    }
    template <typename T>
    std::optional<T> GetMetadata(const std::string &key) const
    {
        auto it = metadata.find(key);
        if (it != metadata.end())
        {
            try
            {
                return std::any_cast<T>(it->second);
            }
            catch (...)
            {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }
};

/**
 * @brief 连接上下文信息
 */
class ConnectionContext
{
public:
    ClientId clientId = 0;
    ClientType clientType = ClientType::UNKNOWN;
    std::string clientName;                     // 客户端名称
    std::string clientVersion;                  // 客户端版本
    std::string ipAddress;                      // IP地址
    int port = 0;                               // 端口号
    std::string sessionId;                      // 会话ID
    std::map<std::string, std::any> attributes; // 扩展属性
    std::vector<std::string> subscribedTopics;  // 订阅的主题列表
    bool isConnected = false;                   // 连接状态
    int64_t lastHeartbeatTime = 0;              // 最后心跳时间
    std::string protocolType;                   // 协议类型

    // 连接质量指标
    struct QualityMetrics
    {
        double latency = 0.0;        // 延迟（毫秒）
        double packetLossRate = 0.0; // 丢包率
        double bandwidth = 0.0;      // 带宽（Mbps）
        int64_t messageCount = 0;    // 消息计数
        int64_t errorCount = 0;      // 错误计数
    } quality;

    // 便捷方法
    template <typename T>
    void SetAttribute(const std::string &key, const T &value)
    {
        attributes[key] = value;
    }

    template <typename T>
    std::optional<T> GetAttribute(const std::string &key) const
    {
        auto it = attributes.find(key);
        if (it != attributes.end())
        {
            try
            {
                return std::any_cast<T>(it->second);
            }
            catch (...)
            {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }

    bool HasAttribute(const std::string &key) const
    {
        return attributes.find(key) != attributes.end();
    }
};

/**
 * @brief 通信配置
 */
struct CommunicationConfig
{
    std::string protocol;                              // 协议类型
    std::string serverAddress;                         // 服务器地址
    int serverPort = 0;                                // 服务器端口
    std::map<std::string, std::string> protocolParams; // 协议特定参数

    // 通用配置
    int heartbeatIntervalMs = 1000; // 心跳间隔
    int connectionTimeoutMs = 5000; // 连接超时时间
    int maxClients = 500;           // 最大客户端数
    bool enableEncryption = false;  // 是否启用加密
    bool enableCompression = false; // 是否启用压缩

    // 性能配置
    int sendBufferSize = 1024 * 1024;      // 发送缓冲区大小
    int receiveBufferSize = 1024 * 1024;   // 接收缓冲区大小
    int maxMessageSize = 10 * 1024 * 1024; // 最大消息大小

    // 扩展配置
    std::map<std::string, std::any> customConfig; // 自定义配置
};

/**
 * @brief 数据接收器接口
 * 用户需要实现此接口来处理接收到的消息
 */
class IDataReceiver
{
public:
    virtual ~IDataReceiver() = default;

    /**
     * @brief 处理传入数据
     */
    virtual void OnDataReceived(const ConnectionContext &context,
                                const NetworkMessage &message) = 0;

    /**
     * @brief 获取感兴趣的消息类型
     */
    virtual std::vector<MessageType> GetInterestedMessageTypes() const = 0;

    /**
     * @brief 获取订阅的主题
     */
    virtual std::vector<std::string> GetSubscribedTopics() const = 0;

    /**
     * @brief 获取接收器名称
     */
    virtual std::string GetReceiverName() const = 0;

    /**
     * @brief 可选的初始化方法
     */
    virtual bool Initialize() { return true; }

    /**
     * @brief 可选的清理方法
     */
    virtual void Cleanup() {}
};

/**
 * @brief 协议适配器接口
 */
class IProtocolAdapter
{
public:
    virtual ~IProtocolAdapter() = default;

    /**
     * @brief 初始化协议适配器
     */
    virtual bool Initialize(const CommunicationConfig &config) = 0;

    /**
     * @brief 启动协议适配器
     */
    virtual bool Start() = 0;

    /**
     * @brief 停止协议适配器
     */
    virtual void Stop() = 0;

    /**
     * @brief 发送消息到指定客户端
     */
    virtual bool SendToClient(ClientId clientId, const NetworkMessage &message) = 0;

    /**
     * @brief 广播消息
     */
    virtual int Broadcast(const NetworkMessage &message) = 0;

    /**
     * @brief 发布消息到主题
     */
    virtual int PublishToTopic(const std::string &topic, const NetworkMessage &message) = 0;

    /**
     * @brief 获取连接的客户端
     */
    virtual std::vector<ConnectionContext> GetConnectedClients() const = 0;

    /**
     * @brief 断开客户端连接
     */
    virtual bool DisconnectClient(ClientId clientId, const std::string &reason = "") = 0;

    /**
     * @brief 获取适配器名称
     */
    virtual std::string GetAdapterName() const = 0;

    /**
     * @brief 获取协议类型
     */
    virtual std::string GetProtocolType() const = 0;

    /**
     * @brief 获取统计信息
     */
    virtual std::map<std::string, uint64_t> GetStatistics() const = 0;

    /**
     * @brief 重置统计信息
     */
    virtual void ResetStatistics() = 0;

    /**
     * @brief 设置消息接收回调
     */
    virtual void SetMessageCallback(std::function<void(const ConnectionContext &,
                                                       const NetworkMessage &)>
                                        callback) = 0;

    /**
     * @brief 设置连接状态回调
     */
    virtual void SetConnectionCallback(std::function<void(const ConnectionContext &,
                                                          bool connected)>
                                           callback) = 0;

    /**
     * @brief 设置错误回调
     */
    virtual void SetErrorCallback(std::function<void(const std::string &, int)> callback) = 0;

    /**
     * @brief 检查适配器是否正在运行
     */
    virtual bool IsRunning() const = 0;
};

/**
 * @brief 通信枢纽抽象接口
 */
class ICommunicationHub
{
public:
    virtual ~ICommunicationHub() = default;

    /**
     * @brief 初始化通信枢纽
     */
    virtual bool Initialize(const std::vector<CommunicationConfig> &configs) = 0;

    /**
     * @brief 启动通信枢纽
     */
    virtual bool Start() = 0;

    /**
     * @brief 停止通信枢纽
     */
    virtual void Stop() = 0;

    /**
     * @brief 完全关闭通信枢纽
     */
    virtual void Shutdown() = 0;

    /**
     * @brief 添加新的协议适配器
     */
    virtual bool AddProtocolAdapter(const CommunicationConfig &config) = 0;

    /**
     * @brief 移除协议适配器
     */
    virtual bool RemoveProtocolAdapter(const std::string &protocolType) = 0;

    /**
     * @brief 检查协议适配器是否存在
     */
    virtual bool HasProtocolAdapter(const std::string &protocolType) const = 0;

    /**
     * @brief 发送消息到指定客户端
     */
    virtual bool SendToClient(ClientId clientId, const NetworkMessage &message) = 0;

    /**
     * @brief 广播消息
     */
    virtual int Broadcast(const NetworkMessage &message) = 0;

    /**
     * @brief 发布消息到主题
     */
    virtual int PublishToTopic(const std::string &topic, const NetworkMessage &message) = 0;

    /**
     * @brief 通过特定协议发送消息
     */
    virtual bool SendViaProtocol(const std::string &protocolType,
                                 ClientId clientId,
                                 const NetworkMessage &message) = 0;

    /**
     * @brief 注册数据接收器
     */
    virtual bool RegisterDataReceiver(std::shared_ptr<IDataReceiver> receiver) = 0;

    /**
     * @brief 注销数据接收器
     */
    virtual bool UnregisterDataReceiver(std::shared_ptr<IDataReceiver> receiver) = 0;

    /**
     * @brief 获取所有连接的客户端
     */
    virtual std::vector<ConnectionContext> GetConnectedClients() const = 0;

    /**
     * @brief 根据客户端ID获取连接上下文
     */
    virtual std::shared_ptr<ConnectionContext> GetClientContext(ClientId clientId) const = 0;

    /**
     * @brief 根据条件查找客户端
     */
    virtual std::vector<std::shared_ptr<ConnectionContext>> FindClients(
        std::function<bool(const ConnectionContext &)> predicate) const = 0;

    /**
     * @brief 断开客户端连接
     */
    virtual bool DisconnectClient(ClientId clientId, const std::string &reason = "") = 0;

    /**
     * @brief 订阅主题
     */
    virtual bool SubscribeToTopic(const std::string &topic,
                                  std::function<void(const ConnectionContext &,
                                                     const NetworkMessage &)>
                                      callback) = 0;

    /**
     * @brief 取消订阅主题
     */
    virtual bool UnsubscribeFromTopic(const std::string &topic) = 0;

    /**
     * @brief 获取统计信息
     */
    virtual std::map<std::string, uint64_t> GetStatistics() const = 0;

    /**
     * @brief 获取支持的协议列表
     */
    virtual std::vector<std::string> GetSupportedProtocols() const = 0;

    /**
     * @brief 获取协议适配器状态
     */
    virtual std::map<std::string, bool> GetProtocolStatus() const = 0;

    /**
     * @brief 设置错误处理回调
     */
    virtual void SetErrorCallback(std::function<void(const std::string &,
                                                     const std::string &)>
                                      callback) = 0;

    /**
     * @brief 获取数据接收器列表
     */
    virtual std::vector<std::string> GetRegisteredReceivers() const = 0;
};

} // namespace CommunicationModule

#endif // COMMUNICATION_HUB_INTERFACE_H