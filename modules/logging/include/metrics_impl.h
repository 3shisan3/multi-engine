#ifndef ME_LOGGING_METRICS_IMPL_H
#define ME_LOGGING_METRICS_IMPL_H

#include "Metrics.h"

#include <map>
#include <mutex>

namespace LogModule
{

/**
 * @brief 指标统计实现
 */
class MetricsImpl : public IMetrics
{
public:
    void Increment(const std::string& name, uint64_t delta = 1) override;
    void Decrement(const std::string& name, uint64_t delta = 1) override;
    void Gauge(const std::string& name, double value) override;
    void Observe(const std::string& name, double value) override;

    MetricValue Get(const std::string& name) const override;
    std::map<std::string, MetricValue> GetAll() const override;

    void Reset() override;
    void Reset(const std::string& name) override;

    std::string Serialize() const override;
    bool Deserialize(const std::string& data) override;

private:
    void Touch(MetricValue& metric);

    mutable std::mutex mutex_;
    std::map<std::string, MetricValue> metrics_;
};

} // namespace LogModule

#endif // ME_LOGGING_METRICS_IMPL_H
