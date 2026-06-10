#ifndef ME_LOGGING_LOGGER_H
#define ME_LOGGING_LOGGER_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "spdlog/spdlog.h"
#include "spdlog/fmt/fmt.h"

namespace LogModule
{

using LogLevel = spdlog::level::level_enum;

enum class LogCategory
{
    SYSTEM,
    SIMULATION,
    ENTITY,
    NETWORK,
    ENVIRONMENT,
    USER,
    PERFORMANCE,
    DEBUG,
    AUDIT
};

struct LoggerConfig
{
    std::string logDirectory = "logs";
    std::string loggerName = "multi_engine";
    size_t maxFileSizeMb = 50;
    size_t maxFiles = 5;
    LogLevel consoleLevel = LogLevel::info;
    LogLevel fileLevel = LogLevel::debug;
    bool enableConsole = true;
    bool enableFile = true;
    bool enableAsync = false;
};

struct LogStatistics
{
    uint64_t totalLogs = 0;
    uint64_t errorLogs = 0;
    uint64_t warningLogs = 0;
};

class ILogger
{
public:
    virtual ~ILogger() = default;

    virtual bool Initialize(const LoggerConfig& config) = 0;
    virtual void Shutdown() = 0;
    virtual void Log(LogLevel level,
                     LogCategory category,
                     const std::string& module,
                     const std::string& file,
                     int line,
                     const std::string& function,
                     const std::string& message,
                     const std::map<std::string, std::string>& context = {}) = 0;
    virtual void SetLogLevel(LogLevel level) = 0;
    virtual LogStatistics GetStatistics() const = 0;
};

class Logger
{
public:
    static ILogger& GetInstance();
    static bool InitializeDefault(const std::string& logDir = "logs");
};

std::string GetCategoryName(LogCategory category);

#define ME_LOG_TRACE(category, module, ...) \
    LogModule::Logger::GetInstance().Log(LogModule::LogLevel::trace, category, module, __FILE__, __LINE__, __FUNCTION__, fmt::format(__VA_ARGS__))

#define ME_LOG_DEBUG(category, module, ...) \
    LogModule::Logger::GetInstance().Log(LogModule::LogLevel::debug, category, module, __FILE__, __LINE__, __FUNCTION__, fmt::format(__VA_ARGS__))

#define ME_LOG_INFO(category, module, ...) \
    LogModule::Logger::GetInstance().Log(LogModule::LogLevel::info, category, module, __FILE__, __LINE__, __FUNCTION__, fmt::format(__VA_ARGS__))

#define ME_LOG_WARN(category, module, ...) \
    LogModule::Logger::GetInstance().Log(LogModule::LogLevel::warn, category, module, __FILE__, __LINE__, __FUNCTION__, fmt::format(__VA_ARGS__))

#define ME_LOG_ERROR(category, module, ...) \
    LogModule::Logger::GetInstance().Log(LogModule::LogLevel::err, category, module, __FILE__, __LINE__, __FUNCTION__, fmt::format(__VA_ARGS__))

} // namespace LogModule

#endif // ME_LOGGING_LOGGER_H