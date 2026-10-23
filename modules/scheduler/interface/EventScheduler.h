/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        EventScheduler.h
Version:     1.0
Author:      cjx
start date: 2026-06-10
Description: 事件调度器接口
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-06-10      cjx        create

*****************************************************************/

#ifndef ME_SCHEDULER_EVENT_SCHEDULER_H
#define ME_SCHEDULER_EVENT_SCHEDULER_H

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace SchedulerModule
{

enum class ScheduledEventType
{
    Timed,
    Conditional,
    Repeating
};

struct ScheduledEvent
{
    std::string id;
    ScheduledEventType type = ScheduledEventType::Timed;
    std::function<void()> callback;
    int64_t triggerTime = 0;
    int64_t intervalMs = 0;
    int64_t nextTriggerTime = 0;
    int repeatCount = -1;
    std::function<bool()> condition;
    bool enabled = true;
};

class EventScheduler
{
public:
    std::string ScheduleTimedEvent(int64_t delayMs, std::function<void()> callback, int64_t currentTime = 0);
    std::string ScheduleRepeatingEvent(int64_t intervalMs, std::function<void()> callback, int repeatCount = -1, int64_t currentTime = 0);
    std::string ScheduleConditionalEvent(std::function<bool()> condition, std::function<void()> callback);
    bool RemoveEvent(const std::string& eventId);
    void Update(int64_t currentTime);
    void Clear();

private:
    std::string GenerateEventId();

    std::vector<std::shared_ptr<ScheduledEvent>> events_;
    std::mutex mutex_;
    uint64_t nextId_ = 1;
};

} // namespace SchedulerModule

#endif // ME_SCHEDULER_EVENT_SCHEDULER_H