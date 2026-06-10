#include "event_bus_impl.h"

#include <chrono>
#include <sstream>

namespace EventModule
{

bool EventBusImpl::Start()
{
    running_ = true;
    return true;
}

void EventBusImpl::Stop()
{
    running_ = false;
}

std::string EventBusImpl::Subscribe(const std::string& eventType, EventCallback callback)
{
    if (eventType.empty() || !callback)
    {
        return "";
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto id = GenerateId("sub");
    subscriptions_[id] = Subscription{id, eventType, std::move(callback)};
    return id;
}

bool EventBusImpl::Unsubscribe(const std::string& subscriptionId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return subscriptions_.erase(subscriptionId) > 0;
}

std::string EventBusImpl::Publish(const std::string& eventType,
                                  const std::any& data,
                                  const std::string& source,
                                  const std::map<std::string, std::any>& metadata)
{
    if (!running_ || eventType.empty())
    {
        return "";
    }

    Event event;
    event.id = GenerateId("evt");
    event.type = eventType;
    event.source = source;
    event.data = data;
    event.metadata = metadata;
    event.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();

    std::vector<EventCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++publishedCount_;
        for (const auto& pair : subscriptions_)
        {
            if (pair.second.eventType == eventType || pair.second.eventType == "*")
            {
                callbacks.push_back(pair.second.callback);
            }
        }
    }

    for (const auto& callback : callbacks)
    {
        callback(event);
        std::lock_guard<std::mutex> lock(mutex_);
        ++deliveredCount_;
    }

    return event.id;
}

std::map<std::string, uint64_t> EventBusImpl::GetStatistics() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return {
        {"subscriptions", subscriptions_.size()},
        {"published_events", publishedCount_},
        {"delivered_events", deliveredCount_}};
}

std::string EventBusImpl::GenerateId(const std::string& prefix)
{
    std::ostringstream stream;
    stream << prefix << "_" << nextId_++;
    return stream.str();
}

std::shared_ptr<IEventBus> CreateEventBus()
{
    return std::make_shared<EventBusImpl>();
}

} // namespace EventModule