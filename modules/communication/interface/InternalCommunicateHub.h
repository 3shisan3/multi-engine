/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        InternalCommunicateHub.h
Version:     1.0
Author:      cjx
start date:
Description: 通用内部模块间通信枢纽接口
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-1-04      cjx            create

*****************************************************************/

#ifndef INTERNAL_COMMUNICATE_HUB_H
#define INTERNAL_COMMUNICATE_HUB_H

#include <any>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "singleton.h"

namespace CommunicationModule
{

/**
 * @brief 内部事件处理器接口
 * 用于模块间通信的事件处理
 */
class IInternalEventHandler
{
public:
    virtual ~IInternalEventHandler() = default;

    /**
     * @brief 处理内部事件
     */
    virtual void HandleInternalEvent(const std::string &eventType,
                                     const std::any &data,
                                     const std::map<std::string, std::any> &metadata) = 0;

    /**
     * @brief 获取处理器感兴趣的事件类型
     */
    virtual std::vector<std::string> GetInterestedEventTypes() const = 0;

    /**
     * @brief 获取处理器名称
     */
    virtual std::string GetHandlerName() const = 0;

    /**
     * @brief 初始化处理器
     */
    virtual bool Initialize() { return true; }

    /**
     * @brief 清理处理器
     */
    virtual void Cleanup() {}
};

/**
 * @brief 模块间通信枢纽
 * 单例模式，负责处理内部模块之间的通信
 */
class InternalCommunicateHub
{
public:
    friend SingletonTemplate<InternalCommunicateHub>;

    InternalCommunicateHub(const InternalCommunicateHub &) = delete;
    InternalCommunicateHub &operator=(const InternalCommunicateHub &) = delete;

    /**
     * @brief 初始化内部通信枢纽
     */
    bool Initialize(const std::map<std::string, std::any> &config = {});

    /**
     * @brief 启动内部通信枢纽
     */
    bool Start();

    /**
     * @brief 停止内部通信枢纽
     */
    void Stop();

    /**
     * @brief 关闭内部通信枢纽
     */
    void Shutdown();

    /**
     * @brief 注册事件处理器
     */
    bool RegisterEventHandler(IInternalEventHandler *handler);

    /**
     * @brief 注销事件处理器
     */
    bool UnregisterEventHandler(IInternalEventHandler *handler);

    /**
     * @brief 发送事件
     */
    std::string SendEvent(const std::string &eventType,
                          const std::any &data = std::any(),
                          const std::string &sourceModule = "",
                          const std::map<std::string, std::any> &metadata = {});

    /**
     * @brief 发送事件并等待响应（请求-响应模式）
     */
    std::future<std::any> SendEventWithResponse(const std::string &eventType,
                                                const std::any &data = std::any(),
                                                const std::string &sourceModule = "",
                                                const std::map<std::string, std::any> &metadata = {},
                                                int timeoutMs = 5000);

    /**
     * @brief 注册响应处理器
     */
    bool RegisterResponseHandler(const std::string &eventType,
                                 std::function<std::any(const std::any &,
                                                        const std::map<std::string, std::any> &)>
                                     handler);

    /**
     * @brief 获取统计信息
     */
    std::map<std::string, uint64_t> GetStatistics() const;

    /**
     * @brief 重置统计信息
     */
    void ResetStatistics();

    /**
     * @brief 设置错误回调
     */
    void SetErrorCallback(std::function<void(const std::string &)> callback);

    /**
     * @brief 检查是否正在运行
     */
    bool IsRunning() const { return running_; }

private:
    struct EventData
    {
        std::string eventId;
        std::string eventType;
        std::any data;
        std::string sourceModule;
        std::map<std::string, std::any> metadata;
        int64_t timestamp;
        std::shared_ptr<std::promise<std::any>> responsePromise;
    };

    struct EventQueueItem
    {
        EventData event;
        std::shared_ptr<std::promise<std::any>> responsePromise;
    };

    InternalCommunicateHub();
    ~InternalCommunicateHub();

    void EventProcessingThread();
    void ProcessEvent(const EventData &event);
    std::string GenerateEventId() const;

    // 事件处理器管理
    mutable std::mutex handlersMutex_;
    std::unordered_map<std::string, std::vector<IInternalEventHandler *>> eventHandlers_;
    std::unordered_map<std::string, IInternalEventHandler *> namedHandlers_;

    // 响应处理器管理
    mutable std::mutex responseHandlersMutex_;
    std::unordered_map<std::string,
                       std::function<std::any(const std::any &, const std::map<std::string, std::any> &)>>
        responseHandlers_;

    // 事件队列
    mutable std::mutex queueMutex_;
    std::condition_variable queueCV_;
    std::queue<EventQueueItem> eventQueue_;

    // 待处理的请求
    mutable std::mutex pendingRequestsMutex_;
    std::unordered_map<std::string, std::shared_ptr<std::promise<std::any>>> pendingRequests_;

    // 工作线程
    std::atomic<bool> running_{false};
    std::vector<std::thread> workerThreads_;
    int numWorkerThreads_ = 4;

    // 统计信息
    mutable std::mutex statsMutex_;
    std::map<std::string, uint64_t> statistics_;

    // 错误回调
    std::function<void(const std::string &)> errorCallback_;
};

} // namespace CommunicationModule

#endif // INTERNAL_COMMUNICATE_HUB_H