#include "InternalCommunicateHub.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

using namespace std::chrono;

namespace CommunicationModule
{

InternalCommunicateHub::InternalCommunicateHub()
{
    ResetStatistics();
}

InternalCommunicateHub::~InternalCommunicateHub()
{
    Shutdown();
}

bool InternalCommunicateHub::Initialize(const std::map<std::string, std::any> &config)
{
    if (running_)
    {
        return false;
    }

    // 解析配置
    if (config.find("worker_threads") != config.end())
    {
        try
        {
            numWorkerThreads_ = std::any_cast<int>(config.at("worker_threads"));
            if (numWorkerThreads_ <= 0 || numWorkerThreads_ > 64)
            {
                numWorkerThreads_ = 4;
            }
        }
        catch (...)
        {
            numWorkerThreads_ = 4;
        }
    }

    // 初始化统计
    ResetStatistics();

    return true;
}

bool InternalCommunicateHub::Start()
{
    if (running_)
    {
        return true;
    }

    running_ = true;

    // 启动工作线程
    try
    {
        for (int i = 0; i < numWorkerThreads_; ++i)
        {
            workerThreads_.emplace_back(&InternalCommunicateHub::EventProcessingThread, this);
        }
    }
    catch (const std::exception &e)
    {
        if (errorCallback_)
        {
            errorCallback_(std::string("Failed to start worker threads: ") + e.what());
        }
        running_ = false;
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        statistics_["startup_time"] = duration_cast<milliseconds>(
                                          system_clock::now().time_since_epoch())
                                          .count();
    }

    return true;
}

void InternalCommunicateHub::Stop()
{
    if (!running_)
    {
        return;
    }

    running_ = false;

    // 通知所有等待的线程
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        queueCV_.notify_all();
    }

    // 等待工作线程结束
    for (auto &thread : workerThreads_)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }
    workerThreads_.clear();
}

void InternalCommunicateHub::Shutdown()
{
    Stop();

    // 清理所有处理器
    {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        eventHandlers_.clear();
        namedHandlers_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(responseHandlersMutex_);
        responseHandlers_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(pendingRequestsMutex_);
        pendingRequests_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        std::queue<EventQueueItem> emptyQueue;
        std::swap(eventQueue_, emptyQueue);
    }

    errorCallback_ = nullptr;
}

bool InternalCommunicateHub::RegisterEventHandler(IInternalEventHandler *handler)
{
    if (!handler || running_)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(handlersMutex_);

    // 检查是否已注册
    if (namedHandlers_.find(handler->GetHandlerName()) != namedHandlers_.end())
    {
        return false;
    }

    // 获取处理器感兴趣的事件类型
    auto interestedTypes = handler->GetInterestedEventTypes();
    if (interestedTypes.empty())
    {
        return false;
    }

    // 为每种事件类型注册处理器
    for (const auto &type : interestedTypes)
    {
        eventHandlers_[type].push_back(handler);
    }

    // 保存命名处理器
    namedHandlers_[handler->GetHandlerName()] = handler;

    // 初始化处理器
    if (!handler->Initialize())
    {
        // 回滚注册
        for (const auto &type : interestedTypes)
        {
            auto &handlers = eventHandlers_[type];
            handlers.erase(std::remove(handlers.begin(), handlers.end(), handler), handlers.end());
        }
        namedHandlers_.erase(handler->GetHandlerName());
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        statistics_["handlers_registered"]++;
    }

    return true;
}

bool InternalCommunicateHub::UnregisterEventHandler(IInternalEventHandler *handler)
{
    if (!handler)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(handlersMutex_);

    // 检查是否已注册
    if (namedHandlers_.find(handler->GetHandlerName()) == namedHandlers_.end())
    {
        return false;
    }

    // 清理处理器
    handler->Cleanup();

    // 从所有事件类型中移除处理器
    for (auto &pair : eventHandlers_)
    {
        auto &handlers = pair.second;
        handlers.erase(std::remove(handlers.begin(), handlers.end(), handler), handlers.end());
    }

    // 从命名处理器中移除
    namedHandlers_.erase(handler->GetHandlerName());

    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        statistics_["handlers_unregistered"]++;
    }

    return true;
}

std::string InternalCommunicateHub::SendEvent(const std::string &eventType,
                                              const std::any &data,
                                              const std::string &sourceModule,
                                              const std::map<std::string, std::any> &metadata)
{
    if (!running_ || eventType.empty())
    {
        return "";
    }

    EventData event;
    event.eventId = GenerateEventId();
    event.eventType = eventType;
    event.data = data;
    event.sourceModule = sourceModule;
    event.metadata = metadata;
    event.timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    // 添加到事件队列
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        eventQueue_.push({event, nullptr});
    }

    // 通知工作线程
    queueCV_.notify_one();

    // 更新统计
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        statistics_["events_sent"]++;
        statistics_["events_by_type_" + eventType]++;
    }

    return event.eventId;
}

std::future<std::any> InternalCommunicateHub::SendEventWithResponse(
    const std::string &eventType,
    const std::any &data,
    const std::string &sourceModule,
    const std::map<std::string, std::any> &metadata,
    int timeoutMs)
{
    std::future<std::any> future;

    if (!running_ || eventType.empty())
    {
        std::promise<std::any> promise;
        promise.set_exception(std::make_exception_ptr(
            std::runtime_error("Communicate hub not running or invalid event type")));
        return promise.get_future();
    }

    EventData event;
    event.eventId = GenerateEventId();
    event.eventType = eventType;
    event.data = data;
    event.sourceModule = sourceModule;
    event.metadata = metadata;
    event.timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    auto responsePromise = std::make_shared<std::promise<std::any>>();
    future = responsePromise->get_future();

    // 保存promise以便后续设置值
    {
        std::lock_guard<std::mutex> lock(pendingRequestsMutex_);
        pendingRequests_[event.eventId] = responsePromise;
    }

    // 添加到事件队列
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        eventQueue_.push({event, responsePromise});
    }

    // 通知工作线程
    queueCV_.notify_one();

    // 设置超时处理
    if (timeoutMs > 0)
    {
        std::thread([eventId = event.eventId, responsePromise, timeoutMs, this]()
                    {
            std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
            
            std::lock_guard<std::mutex> lock(pendingRequestsMutex_);
            auto it = pendingRequests_.find(eventId);
            if (it != pendingRequests_.end() && it->second == responsePromise) {
                pendingRequests_.erase(it);
                try {
                    responsePromise->set_exception(std::make_exception_ptr(
                        std::runtime_error("Request timeout")));
                } catch (...) {
                    // promise可能已经被设置
                }
                
                std::lock_guard<std::mutex> statsLock(statsMutex_);
                statistics_["request_timeouts"]++;
            } })
            .detach();
    }

    // 更新统计
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        statistics_["requests_sent"]++;
        statistics_["requests_by_type_" + eventType]++;
    }

    return future;
}

bool InternalCommunicateHub::RegisterResponseHandler(
    const std::string &eventType,
    std::function<std::any(const std::any &, const std::map<std::string, std::any> &)> handler)
{
    if (eventType.empty() || !handler)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(responseHandlersMutex_);
    responseHandlers_[eventType] = handler;

    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        statistics_["response_handlers_registered"]++;
    }

    return true;
}

std::map<std::string, uint64_t> InternalCommunicateHub::GetStatistics() const
{
    std::lock_guard<std::mutex> lock(statsMutex_);
    return statistics_;
}

void InternalCommunicateHub::ResetStatistics()
{
    std::lock_guard<std::mutex> lock(statsMutex_);
    statistics_.clear();
    statistics_["events_sent"] = 0;
    statistics_["requests_sent"] = 0;
    statistics_["handlers_registered"] = 0;
    statistics_["handlers_unregistered"] = 0;
    statistics_["response_handlers_registered"] = 0;
    statistics_["request_timeouts"] = 0;
    statistics_["queue_size"] = 0;
    statistics_["processing_errors"] = 0;
}

void InternalCommunicateHub::SetErrorCallback(std::function<void(const std::string &)> callback)
{
    errorCallback_ = callback;
}

void InternalCommunicateHub::EventProcessingThread()
{
    while (running_)
    {
        EventQueueItem item;

        // 从队列获取事件
        {
            std::unique_lock<std::mutex> lock(queueMutex_);

            queueCV_.wait(lock, [this]()
                          { return !eventQueue_.empty() || !running_; });

            if (!running_ && eventQueue_.empty())
            {
                break;
            }

            if (!eventQueue_.empty())
            {
                item = eventQueue_.front();
                eventQueue_.pop();
            }
            else
            {
                continue;
            }

            // 更新队列大小统计
            {
                std::lock_guard<std::mutex> statsLock(statsMutex_);
                statistics_["queue_size"] = eventQueue_.size();
            }
        }

        // 处理事件
        try
        {
            ProcessEvent(item.event);

            // 如果是请求事件且有响应处理器，处理响应
            if (item.responsePromise)
            {
                std::function<std::any(const std::any &, const std::map<std::string, std::any> &)> responseHandler;

                {
                    std::lock_guard<std::mutex> lock(responseHandlersMutex_);
                    auto it = responseHandlers_.find(item.event.eventType);
                    if (it != responseHandlers_.end())
                    {
                        responseHandler = it->second;
                    }
                }

                if (responseHandler)
                {
                    try
                    {
                        auto responseData = responseHandler(item.event.data, item.event.metadata);
                        item.responsePromise->set_value(responseData);
                    }
                    catch (const std::exception &e)
                    {
                        item.responsePromise->set_exception(std::current_exception());
                        if (errorCallback_)
                        {
                            errorCallback_(std::string("Error in response handler: ") + e.what());
                        }
                    }
                }
                else
                {
                    // 没有响应处理器，设置默认值
                    item.responsePromise->set_value(std::any());
                }

                // 从pending请求中移除
                {
                    std::lock_guard<std::mutex> lock(pendingRequestsMutex_);
                    pendingRequests_.erase(item.event.eventId);
                }
            }
        }
        catch (const std::exception &e)
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            statistics_["processing_errors"]++;

            if (errorCallback_)
            {
                errorCallback_(std::string("Error processing event: ") + e.what());
            }
        }
    }
}

void InternalCommunicateHub::ProcessEvent(const EventData &event)
{
    std::vector<IInternalEventHandler *> handlersToNotify;

    // 查找处理该事件类型的处理器
    {
        std::lock_guard<std::mutex> lock(handlersMutex_);
        auto it = eventHandlers_.find(event.eventType);
        if (it != eventHandlers_.end())
        {
            handlersToNotify = it->second;
        }
    }

    // 通知所有处理器
    for (auto *handler : handlersToNotify)
    {
        try
        {
            handler->HandleInternalEvent(event.eventType, event.data, event.metadata);
        }
        catch (const std::exception &e)
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            statistics_["handler_errors"]++;

            if (errorCallback_)
            {
                errorCallback_(std::string("Error in handler ") + handler->GetHandlerName() +
                               ": " + e.what());
            }
        }
    }

    // 更新处理统计
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        statistics_["events_processed"]++;
        if (handlersToNotify.empty())
        {
            statistics_["events_no_handler"]++;
        }
    }
}

std::string InternalCommunicateHub::GenerateEventId() const
{
    static std::atomic<uint64_t> counter{0};
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    auto now = system_clock::now();
    auto timestamp = duration_cast<milliseconds>(now.time_since_epoch()).count();

    uint64_t random = dis(gen);
    uint64_t seq = ++counter;

    std::stringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(8) << (timestamp & 0xFFFFFFFF) << "-"
       << std::setw(4) << ((timestamp >> 32) & 0xFFFF) << "-"
       << std::setw(4) << (random & 0xFFFF) << "-"
       << std::setw(4) << ((random >> 16) & 0xFFFF) << "-"
       << std::setw(8) << seq;

    return ss.str();
}

} // namespace CommunicationModule