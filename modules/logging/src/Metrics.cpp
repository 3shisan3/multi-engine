/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        Metrics.cpp
Description: 指标统计实现（日志模块内聚能力）

*****************************************************************/

#include "metrics_impl.h"

#include <nlohmann/json.hpp>

#include <chrono>

using json = nlohmann::json;
using namespace std::chrono;

namespace LogModule
{

namespace
{

int64_t NowMs()
{
    return duration_cast<milliseconds>(
               system_clock::now().time_since_epoch())
        .count();
}

} // namespace

void MetricsImpl::Touch(MetricValue& metric)
{
    metric.count++;
    metric.lastTimestamp = NowMs();
}

void MetricsImpl::Increment(const std::string& name, uint64_t delta)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto& metric = metrics_[name];
    Touch(metric);
    metric.counter += delta;
}

void MetricsImpl::Decrement(const std::string& name, uint64_t delta)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto& metric = metrics_[name];
    Touch(metric);
    metric.counter = metric.counter > delta ? metric.counter - delta : 0;
}

void MetricsImpl::Gauge(const std::string& name, double value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto& metric = metrics_[name];
    Touch(metric);
    metric.gauge = value;
}

void MetricsImpl::Observe(const std::string& name, double value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto& metric = metrics_[name];
    Touch(metric);
    metric.sum += value;

    if (metric.count == 1)
    {
        metric.min = value;
        metric.max = value;
    }
    else
    {
        if (value < metric.min)
        {
            metric.min = value;
        }
        if (value > metric.max)
        {
            metric.max = value;
        }
    }
}

MetricValue MetricsImpl::Get(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = metrics_.find(name);
    return it == metrics_.end() ? MetricValue{} : it->second;
}

std::map<std::string, MetricValue> MetricsImpl::GetAll() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return metrics_;
}

void MetricsImpl::Reset()
{
    std::lock_guard<std::mutex> lock(mutex_);
    metrics_.clear();
}

void MetricsImpl::Reset(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    metrics_.erase(name);
}

std::string MetricsImpl::Serialize() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    json root = json::object();
    for (const auto& pair : metrics_)
    {
        const MetricValue& metric = pair.second;
        root[pair.first] = {
            {"count", metric.count},
            {"counter", metric.counter},
            {"gauge", metric.gauge},
            {"sum", metric.sum},
            {"min", metric.min},
            {"max", metric.max},
            {"lastTimestamp", metric.lastTimestamp}};
    }

    return root.dump(2);
}

bool MetricsImpl::Deserialize(const std::string& data)
{
    json root;
    try
    {
        root = json::parse(data);
    }
    catch (const json::exception&)
    {
        return false;
    }

    if (!root.is_object())
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    metrics_.clear();

    for (auto it = root.begin(); it != root.end(); ++it)
    {
        const json& value = it.value();
        MetricValue metric;
        metric.count = value.value("count", 0);
        metric.counter = value.value("counter", 0);
        metric.gauge = value.value("gauge", 0.0);
        metric.sum = value.value("sum", 0.0);
        metric.min = value.value("min", 0.0);
        metric.max = value.value("max", 0.0);
        metric.lastTimestamp = value.value("lastTimestamp", static_cast<int64_t>(0));
        metrics_[it.key()] = metric;
    }

    return true;
}

std::shared_ptr<IMetrics> CreateMetrics()
{
    return std::make_shared<MetricsImpl>();
}

ScopedTimer::ScopedTimer(IMetrics& metrics, const std::string& name)
    : metrics_(metrics), name_(name), start_(NowMs())
{
}

ScopedTimer::~ScopedTimer()
{
    metrics_.Observe(name_, static_cast<double>(NowMs() - start_));
}

} // namespace LogModule
