#include "PluginBase.h"

#include <chrono>
#include <iostream>

using namespace std::chrono;

namespace PluginSystem
{

PluginBase::PluginBase(const std::string& pluginId,
                       const std::string& pluginType,
                       const std::string& pluginName,
                       const std::string& version)
{
    info_.pluginId = pluginId;
    info_.pluginType = pluginType;
    info_.pluginName = pluginName.empty() ? pluginId : pluginName;
    info_.pluginVersion = version;
    info_.description = "A plugin implementation";
    info_.author = "Unknown";
    info_.license = "MIT";
}

PluginBase::~PluginBase()
{
    Stop();
    Unload();
}

PluginInfo PluginBase::GetPluginInfo() const
{
    return info_;
}

bool PluginBase::Initialize(const std::map<std::string, std::any>& config)
{
    if (isInitialized_)
    {
        return true;
    }
    
    UpdateConfig(config);
    
    bool success = OnInitialize(config);
    if (success)
    {
        isInitialized_ = true;
        LogInfo("Plugin initialized: " + info_.pluginId);
    }
    else
    {
        LogError("Failed to initialize plugin: " + info_.pluginId);
    }
    
    return success;
}

bool PluginBase::Start()
{
    if (!isInitialized_)
    {
        LogError("Plugin not initialized: " + info_.pluginId);
        return false;
    }
    
    if (isRunning_)
    {
        return true;
    }
    
    bool success = OnStart();
    if (success)
    {
        isRunning_ = true;
        info_.isEnabled = true;
        LogInfo("Plugin started: " + info_.pluginId);
        EmitEvent(PluginEvent::STARTED);
    }
    else
    {
        LogError("Failed to start plugin: " + info_.pluginId);
    }
    
    return success;
}

void PluginBase::Stop()
{
    if (!isRunning_)
    {
        return;
    }
    
    OnStop();
    isRunning_ = false;
    info_.isEnabled = false;
    
    LogInfo("Plugin stopped: " + info_.pluginId);
    EmitEvent(PluginEvent::STOPPED);
}

void PluginBase::Unload()
{
    if (!isInitialized_)
    {
        return;
    }
    
    Stop();
    OnUnload();
    isInitialized_ = false;
    
    LogInfo("Plugin unloaded: " + info_.pluginId);
    EmitEvent(PluginEvent::UNLOADED);
}

std::string PluginBase::GetStatus() const
{
    std::string status = "Plugin: " + info_.pluginId + "\n";
    status += "Name: " + info_.pluginName + "\n";
    status += "Type: " + info_.pluginType + "\n";
    status += "Version: " + info_.pluginVersion + "\n";
    status += "Initialized: " + std::to_string(isInitialized_) + "\n";
    status += "Running: " + std::to_string(isRunning_) + "\n";
    status += "Enabled: " + std::to_string(info_.isEnabled) + "\n";
    
    return status;
}

std::any PluginBase::ExecuteCommand(const std::string& command,
                                   const std::map<std::string, std::any>& params)
{
    if (!isInitialized_)
    {
        LogError("Plugin not initialized: " + info_.pluginId);
        return std::any();
    }
    
    try
    {
        return OnExecuteCommand(command, params);
    }
    catch (const std::exception& e)
    {
        LogError("Error executing command: " + std::string(e.what()));
        return std::any();
    }
}

std::string PluginBase::GetPluginType() const
{
    return info_.pluginType;
}

void PluginBase::SetPluginInfo(const PluginInfo& info)
{
    info_ = info;
}

void PluginBase::AddDependency(const std::string& dependency)
{
    info_.dependencies.push_back(dependency);
}

void PluginBase::SetDescription(const std::string& description)
{
    info_.description = description;
}

void PluginBase::SetAuthor(const std::string& author)
{
    info_.author = author;
}

void PluginBase::SetLicense(const std::string& license)
{
    info_.license = license;
}

void PluginBase::SetLogger(std::function<void(const std::string&, int)> logger)
{
    logger_ = logger;
}

void PluginBase::LogInfo(const std::string& message) const
{
    if (logger_)
    {
        logger_("[INFO] " + info_.pluginId + ": " + message, 0);
    }
    else
    {
        std::cout << "[INFO] " << info_.pluginId << ": " << message << std::endl;
    }
}

void PluginBase::LogWarning(const std::string& message) const
{
    if (logger_)
    {
        logger_("[WARN] " + info_.pluginId + ": " + message, 1);
    }
    else
    {
        std::cout << "[WARN] " << info_.pluginId << ": " << message << std::endl;
    }
}

void PluginBase::LogError(const std::string& message) const
{
    if (logger_)
    {
        logger_("[ERROR] " + info_.pluginId + ": " + message, 2);
    }
    else
    {
        std::cerr << "[ERROR] " << info_.pluginId << ": " << message << std::endl;
    }
}

void PluginBase::SetEventCallback(PluginEventCallback callback)
{
    eventCallback_ = callback;
}

void PluginBase::EmitEvent(PluginEvent event, const std::any& data)
{
    if (eventCallback_)
    {
        PluginEventData eventData;
        eventData.pluginId = info_.pluginId;
        eventData.event = event;
        eventData.data = data;
        eventData.timestamp = duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()).count();
        
        eventCallback_(eventData);
    }
}

template<typename T>
T PluginBase::GetConfigValue(const std::string& key, const T& defaultValue) const
{
    std::lock_guard<std::mutex> lock(configMutex_);
    
    auto it = config_.find(key);
    if (it == config_.end())
    {
        return defaultValue;
    }
    
    try
    {
        return std::any_cast<T>(it->second);
    }
    catch (...)
    {
        return defaultValue;
    }
}

void PluginBase::UpdateConfig(const std::map<std::string, std::any>& newConfig)
{
    std::lock_guard<std::mutex> lock(configMutex_);
    
    for (const auto& pair : newConfig)
    {
        config_[pair.first] = pair.second;
    }
}

} // namespace PluginSystem