/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        Metrics.h
Version:     1.0
Author:      cjx
start date: 2026-10-23
Description: 指标统计接口（日志模块内聚能力）
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-10-23      cjx        create

*****************************************************************/

#ifndef ME_LOGGING_METRICS_H
#define ME_LOGGING_METRICS_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>

namespace LogModule
{

/**
 * @brief 指标聚合值
 *
 * 统一承载计数器、仪表盘与观测值（如延迟、尺寸）的统计信息。
 */
struct MetricValue
{
    uint64_t count = 0;        // 更新次数
    uint64_t counter = 0;      // 计数器累加值（Increment/Decrement 修改）
    double gauge = 0.0;        // 仪表盘当前值（Gauge 修改）
    double sum = 0.0;          // 观测值累计（Observe 修改）
    double min = 0.0;          // 观测值最小值
    double max = 0.0;          // 观测值最大值
    int64_t lastTimestamp = 0; // 最后更新时间（毫秒）
};

/**
 * @brief 指标统计接口
 *
 * 线程安全，可在多线程环境下使用。适用于：
 * - 通信消息计数、错误计数
 * - 调度耗时、主循环帧率
 * - 连接数、队列长度等瞬时值
 */
class IMetrics
{
public:
    virtual ~IMetrics() = default;

    /** @brief 计数器自增 */
    virtual void Increment(const std::string& name, uint64_t delta = 1) = 0;

    /** @brief 计数器自减（不会低于 0） */
    virtual void Decrement(const std::string& name, uint64_t delta = 1) = 0;

    /** @brief 设置仪表盘瞬时值 */
    virtual void Gauge(const std::string& name, double value) = 0;

    /** @brief 记录一次观测值（用于延迟、尺寸等，自动维护 sum/min/max） */
    virtual void Observe(const std::string& name, double value) = 0;

    /** @brief 获取单个指标，不存在时返回默认值 */
    virtual MetricValue Get(const std::string& name) const = 0;

    /** @brief 获取全部指标快照 */
    virtual std::map<std::string, MetricValue> GetAll() const = 0;

    /** @brief 清空全部指标 */
    virtual void Reset() = 0;

    /** @brief 清空指定指标 */
    virtual void Reset(const std::string& name) = 0;

    /** @brief 序列化为 JSON 字符串 */
    virtual std::string Serialize() const = 0;

    /** @brief 从 JSON 字符串恢复，成功返回 true */
    virtual bool Deserialize(const std::string& data) = 0;
};

/**
 * @brief 创建指标统计实例
 */
std::shared_ptr<IMetrics> CreateMetrics();

/**
 * @brief RAII 计时器
 *
 * 构造时开始计时，析构时自动将耗时（毫秒）记录到指定指标。
 * 典型用法：
 * @code
 *   {
 *       LogModule::ScopedTimer timer(metrics, "step.cost");
 *       DoWork();
 *   }
 * @endcode
 */
class ScopedTimer
{
public:
    ScopedTimer(IMetrics& metrics, const std::string& name);
    ~ScopedTimer();

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    IMetrics& metrics_;
    std::string name_;
    int64_t start_;
};

} // namespace LogModule

#endif // ME_LOGGING_METRICS_H
