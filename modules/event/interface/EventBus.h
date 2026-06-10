#ifndef ME_EVENT_BUS_H
#define ME_EVENT_BUS_H

#include <any>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace EventModule
{

struct Event
{
    std::string id;
    std::string type;
    std::string source;
    std::any data;
    std::map<std::string, std::any> metadata;
    int64_t timestamp = 0;
};

using EventCallback = std::function<void(const Event&)>;

class IEventBus
{
public:
    virtual ~IEventBus() = default;

    virtual bool Start() = 0;
    virtual void Stop() = 0;
    virtual std::string Subscribe(const std::string& eventType, EventCallback callback) = 0;
    virtual bool Unsubscribe(const std::string& subscriptionId) = 0;
    virtual std::string Publish(const std::string& eventType,
                                const std::any& data = std::any(),
                                const std::string& source = "",
                                const std::map<std::string, std::any>& metadata = {}) = 0;
    virtual std::map<std::string, uint64_t> GetStatistics() const = 0;
};

std::shared_ptr<IEventBus> CreateEventBus();

} // namespace EventModule

#endif // ME_EVENT_BUS_H