#ifndef ME_EVENT_BUS_IMPL_H
#define ME_EVENT_BUS_IMPL_H

#include "EventBus.h"

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace EventModule
{

class EventBusImpl : public IEventBus
{
public:
    bool Start() override;
    void Stop() override;
    std::string Subscribe(const std::string& eventType, EventCallback callback) override;
    bool Unsubscribe(const std::string& subscriptionId) override;
    std::string Publish(const std::string& eventType,
                        const std::any& data = std::any(),
                        const std::string& source = "",
                        const std::map<std::string, std::any>& metadata = {}) override;
    std::map<std::string, uint64_t> GetStatistics() const override;

private:
    struct Subscription
    {
        std::string id;
        std::string eventType;
        EventCallback callback;
    };

    std::string GenerateId(const std::string& prefix);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Subscription> subscriptions_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> nextId_{1};
    uint64_t publishedCount_ = 0;
    uint64_t deliveredCount_ = 0;
};

} // namespace EventModule

#endif // ME_EVENT_BUS_IMPL_H