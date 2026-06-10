#include "logger_impl.h"

#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include <filesystem>
#include <sstream>
#include <vector>

namespace LogModule
{

std::string GetCategoryName(LogCategory category)
{
    switch (category)
    {
    case LogCategory::SYSTEM:
        return "system";
    case LogCategory::SIMULATION:
        return "simulation";
    case LogCategory::ENTITY:
        return "entity";
    case LogCategory::NETWORK:
        return "network";
    case LogCategory::ENVIRONMENT:
        return "environment";
    case LogCategory::USER:
        return "user";
    case LogCategory::PERFORMANCE:
        return "performance";
    case LogCategory::DEBUG:
        return "debug";
    case LogCategory::AUDIT:
        return "audit";
    }
    return "unknown";
}

bool LoggerImpl::Initialize(const LoggerConfig& config)
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<spdlog::sink_ptr> sinks;

    if (config.enableConsole)
    {
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_level(config.consoleLevel);
        sinks.push_back(consoleSink);
    }

    if (config.enableFile)
    {
        std::filesystem::create_directories(config.logDirectory);
        auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            config.logDirectory + "/" + config.loggerName + ".log",
            config.maxFileSizeMb * 1024 * 1024,
            config.maxFiles);
        fileSink->set_level(config.fileLevel);
        sinks.push_back(fileSink);
    }

    if (sinks.empty())
    {
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    }

    logger_ = std::make_shared<spdlog::logger>(config.loggerName, sinks.begin(), sinks.end());
    logger_->set_level(std::min(config.consoleLevel, config.fileLevel));
    logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");
    spdlog::register_logger(logger_);
    return true;
}

void LoggerImpl::Shutdown()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (logger_)
    {
        logger_->flush();
        spdlog::drop(logger_->name());
        logger_.reset();
    }
}

void LoggerImpl::Log(LogLevel level,
                     LogCategory category,
                     const std::string& module,
                     const std::string& file,
                     int line,
                     const std::string& function,
                     const std::string& message,
                     const std::map<std::string, std::string>& context)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!logger_)
    {
        LoggerConfig config;
        Initialize(config);
    }

    std::ostringstream stream;
    stream << "[" << GetCategoryName(category) << "] [" << module << "] " << message
           << " (" << file << ":" << line << " " << function << ")";
    if (!context.empty())
    {
        stream << " {";
        bool first = true;
        for (const auto& pair : context)
        {
            if (!first)
            {
                stream << ", ";
            }
            stream << pair.first << "=" << pair.second;
            first = false;
        }
        stream << "}";
    }

    logger_->log(level, stream.str());
    ++statistics_.totalLogs;
    if (level == LogLevel::err || level == LogLevel::critical)
    {
        ++statistics_.errorLogs;
    }
    if (level == LogLevel::warn)
    {
        ++statistics_.warningLogs;
    }
}

void LoggerImpl::SetLogLevel(LogLevel level)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (logger_)
    {
        logger_->set_level(level);
    }
}

LogStatistics LoggerImpl::GetStatistics() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return statistics_;
}

ILogger& Logger::GetInstance()
{
    static LoggerImpl logger;
    return logger;
}

bool Logger::InitializeDefault(const std::string& logDir)
{
    LoggerConfig config;
    config.logDirectory = logDir;
    return GetInstance().Initialize(config);
}

} // namespace LogModule