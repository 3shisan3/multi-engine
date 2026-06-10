#include "EventScheduler.h"

#include <algorithm>
#include <sstream>

namespace SchedulerModule
{

std::string EventScheduler::ScheduleTimedEvent(int64_t delayMs, std::function<void()> callback, int64_t currentTime)
{
    if (!callback)
    {
        return "";
    }

    auto event = std::make_shared<ScheduledEvent>();
    event->id = GenerateEventId();
    event->type = ScheduledEventType::Timed;
    event->callback = std::move(callback);
    event->triggerTime = currentTime + delayMs;

    std::lock_guard<std::mutex> lock(mutex_);
    events_.push_back(event);
    return event->id;
}

std::string EventScheduler::ScheduleRepeatingEvent(int64_t intervalMs, std::function<void()> callback, int repeatCount, int64_t currentTime)
{
    if (!callback || intervalMs <= 0)
    {
        return "";
    }

    auto event = std::make_shared<ScheduledEvent>();
    event->id = GenerateEventId();
    event->type = ScheduledEventType::Repeating;
    event->callback = std::move(callback);
    event->intervalMs = intervalMs;
    event->nextTriggerTime = currentTime + intervalMs;
    event->repeatCount = repeatCount;

    std::lock_guard<std::mutex> lock(mutex_);
    events_.push_back(event);
    return event->id;
}

std::string EventScheduler::ScheduleConditionalEvent(std::function<bool()> condition, std::function<void()> callback)
{
    if (!condition || !callback)
    {
        return "";
    }

    auto event = std::make_shared<ScheduledEvent>();
    event->id = GenerateEventId();
    event->type = ScheduledEventType::Conditional;
    event->condition = std::move(condition);
    event->callback = std::move(callback);

    std::lock_guard<std::mutex> lock(mutex_);
    events_.push_back(event);
    return event->id;
}

bool EventScheduler::RemoveEvent(const std::string& eventId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto oldSize = events_.size();
    events_.erase(std::remove_if(events_.begin(), events_.end(), [&](const auto& event) {
        return event->id == eventId;
    }), events_.end());
    return events_.size() != oldSize;
}

void EventScheduler::Update(int64_t currentTime)
{
    std::vector<std::function<void()>> callbacks;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& event : events_)
        {
            if (!event->enabled)
            {
                continue;
            }

            bool triggered = false;
            if (event->type == ScheduledEventType::Timed && currentTime >= event->triggerTime)
            {
                triggered = true;
                event->enabled = false;
            }
            else if (event->type == ScheduledEventType::Repeating && currentTime >= event->nextTriggerTime)
            {
                triggered = true;
                event->nextTriggerTime = currentTime + event->intervalMs;
                if (event->repeatCount > 0)
                {
                    --event->repeatCount;
                    if (event->repeatCount == 0)
                    {
                        event->enabled = false;
                    }
                }
            }
            else if (event->type == ScheduledEventType::Conditional && event->condition && event->condition())
            {
                triggered = true;
                event->enabled = false;
            }

            if (triggered)
            {
                callbacks.push_back(event->callback);
            }
        }

        events_.erase(std::remove_if(events_.begin(), events_.end(), [](const auto& event) {
            return !event->enabled;
        }), events_.end());
    }

    for (const auto& callback : callbacks)
    {
        callback();
    }
}

void EventScheduler::Clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    events_.clear();
}

std::string EventScheduler::GenerateEventId()
{
    std::ostringstream stream;
    stream << "scheduled_" << nextId_++;
    return stream.str();
}

} // namespace SchedulerModule