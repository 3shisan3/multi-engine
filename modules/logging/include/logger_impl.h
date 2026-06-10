#ifndef ME_LOGGING_LOGGER_IMPL_H
#define ME_LOGGING_LOGGER_IMPL_H

#include "Logger.h"

#include <memory>
#include <mutex>

namespace LogModule
{

class LoggerImpl : public ILogger
{
public:
    bool Initialize(const LoggerConfig& config) override;
    void Shutdown() override;
    void Log(LogLevel level,
             LogCategory category,
             const std::string& module,
             const std::string& file,
             int line,
             const std::string& function,
             const std::string& message,
             const std::map<std::string, std::string>& context = {}) override;
    void SetLogLevel(LogLevel level) override;
    LogStatistics GetStatistics() const override;

private:
    mutable std::mutex mutex_;
    std::shared_ptr<spdlog::logger> logger_;
    LogStatistics statistics_;
};

} // namespace LogModule

#endif // ME_LOGGING_LOGGER_IMPL_H