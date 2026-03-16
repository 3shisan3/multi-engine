#include "dynamiclib_loader_impl.h"
#include "plugin_manager_impl.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace fs = std::filesystem;
using namespace std::chrono;

namespace PluginSystem
{

// =============== PluginManagerImpl 实现 ===============

PluginManagerImpl::PluginManagerImpl()
    : nextPluginId_(1)
{
}

PluginManagerImpl::~PluginManagerImpl()
{
    StopAllPlugins();
    
    // 清理所有插件
    std::unique_lock lock(pluginsMutex_);
    for (auto& pair : plugins_)
    {
        try
        {
            pair.second.plugin->Unload();
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error unloading plugin " << pair.first << ": " << e.what() << std::endl;
        }
    }
    plugins_.clear();
    libraries_.clear();
}

bool PluginManagerImpl::Initialize(const std::string &configDir)
{
    if (initialized_.load())
    {
        return true;
    }

    configDir_ = configDir;

    // 确保配置目录存在
    if (!configDir.empty())
    {
        try
        {
            fs::create_directories(configDir);
        }
        catch (const fs::filesystem_error &e)
        {
            std::cerr << "Failed to create config directory: " << e.what() << std::endl;
            return false;
        }
    }

    initialized_.store(true);
    return true;
}

std::string PluginManagerImpl::LoadPlugin(const std::string &pluginPath)
{
    if (!initialized_.load())
    {
        std::cerr << "Plugin manager not initialized" << std::endl;
        return "";
    }

    if (!fs::exists(pluginPath))
    {
        std::cerr << "Plugin file does not exist: " << pluginPath << std::endl;
        return "";
    }

    PluginEntry entry;
    if (!LoadPluginInternal(pluginPath, entry))
    {
        return "";
    }

    std::string pluginId = entry.info.pluginId;

    {
        std::unique_lock lock(pluginsMutex_);
        plugins_[pluginId] = std::move(entry);
    }

    // 加载配置
    LoadPluginConfig(pluginId);

    NotifyPluginEvent(pluginId, PluginEvent::LOADED);

    return pluginId;
}

bool PluginManagerImpl::LoadPluginInternal(const std::string &pluginPath, PluginEntry &entry)
{
    // 检查是否已加载相同路径的库
    std::string libraryKey;
    try
    {
        libraryKey = fs::canonical(pluginPath).string();
    }
    catch (const fs::filesystem_error &)
    {
        libraryKey = pluginPath;
    }

    std::shared_ptr<IDynamicLibraryLoader> loader;

    // 先检查库是否已加载
    {
        std::shared_lock lock(pluginsMutex_);
        auto libIt = libraries_.find(libraryKey);
        if (libIt != libraries_.end())
        {
            // 库已加载，重用加载器
            loader = libIt->second.loader;

            // 检查是否已创建过相同名称的插件实例
            std::string pluginName = fs::path(pluginPath).stem().string();
            for (const auto &[pid, plugin] : libIt->second.plugins)
            {
                if (plugin->GetPluginInfo().pluginName == pluginName)
                {
                    std::cerr << "Plugin with same name already loaded: " << pluginName << std::endl;
                    return false;
                }
            }
        }
    }

    // 需要加载新库
    if (!loader)
    {
        loader = CreateDynamicLibraryLoader();
        if (!loader->Load(pluginPath))
        {
            std::cerr << "Failed to load library: " << loader->GetLastError() << std::endl;
            return false;
        }

        // 保存库信息
        LibraryEntry libEntry;
        libEntry.loader = loader;

        std::unique_lock lock(pluginsMutex_);
        libraries_[libraryKey] = std::move(libEntry);
    }

    // 获取创建和销毁函数
    // 使用 GetSymbol 模板方法
    using CreateFuncType = IPlugin* (*)();
    using DestroyFuncType = void (*)(IPlugin*);
    
    auto createFuncWrapper = loader->GetSymbol<CreateFuncType>("CreatePlugin");
    auto destroyFuncWrapper = loader->GetSymbol<DestroyFuncType>("DestroyPlugin");

    // 检查是否成功获取创建函数
    if (!createFuncWrapper)
    {
        std::cerr << "Failed to get CreatePlugin function" << std::endl;
        return false;
    }

    // 获取原始函数指针
    CreateFuncType createFunc = nullptr;
    if (createFuncWrapper)
    {
        // std::function 可以隐式转换为函数指针吗？不能，需要特殊处理
        // 更好的方式是直接使用函数指针
        createFunc = *createFuncWrapper.target<CreateFuncType>();
    }

    if (!createFunc)
    {
        std::cerr << "Failed to get raw CreatePlugin function pointer" << std::endl;
        return false;
    }

    // 创建插件实例
    IPlugin *rawPlugin = createFunc();
    if (!rawPlugin)
    {
        std::cerr << "Failed to create plugin instance" << std::endl;
        return false;
    }

    // 获取销毁函数指针
    DestroyFuncType destroyFunc = nullptr;
    if (destroyFuncWrapper)
    {
        destroyFunc = *destroyFuncWrapper.target<DestroyFuncType>();
    }

    // 包装插件实例，设置自定义删除器
    if (destroyFunc)
    {
        entry.plugin = std::shared_ptr<IPlugin>(rawPlugin, 
            [destroyFunc](IPlugin *plugin) {
                if (plugin)
                {
                    destroyFunc(plugin);
                }
            });
    }
    else
    {
        // 如果没有销毁函数，使用默认删除器
        std::cerr << "Warning: No DestroyPlugin function found, using default deleter" << std::endl;
        entry.plugin = std::shared_ptr<IPlugin>(rawPlugin, [](IPlugin*) {});
    }

    // 获取插件信息
    entry.info = entry.plugin->GetPluginInfo();
    entry.info.libraryPath = pluginPath;
    entry.info.isLoaded = true;

    // 如果没有插件ID，生成一个
    if (entry.info.pluginId.empty())
    {
        entry.info.pluginId = "plugin_" + std::to_string(nextPluginId_++);
    }

    // 如果插件名称为空，使用文件名
    if (entry.info.pluginName.empty())
    {
        entry.info.pluginName = fs::path(pluginPath).stem().string();
    }

    entry.loader = loader;

    // 保存到库的插件列表
    {
        std::unique_lock lock(pluginsMutex_);
        libraries_[libraryKey].plugins[entry.info.pluginId] = entry.plugin;
    }

    return true;
}

bool PluginManagerImpl::UnloadPlugin(const std::string &pluginId)
{
    if (!initialized_.load())
    {
        return false;
    }

    std::shared_ptr<IPlugin> plugin;
    std::string libraryPath;
    PluginInfo info;

    {
        std::unique_lock lock(pluginsMutex_);
        auto it = plugins_.find(pluginId);
        if (it == plugins_.end())
        {
            return false;
        }

        plugin = it->second.plugin;
        libraryPath = it->second.info.libraryPath;
        info = it->second.info;

        plugins_.erase(it);
    }

    // 停止并卸载插件
    if (plugin)
    {
        try
        {
            plugin->Stop();
            plugin->Unload();
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error during plugin unload: " << e.what() << std::endl;
        }
    }

    // 清理库引用
    std::string libraryKey;
    try
    {
        libraryKey = fs::canonical(libraryPath).string();
    }
    catch (const fs::filesystem_error &)
    {
        libraryKey = libraryPath;
    }

    {
        std::unique_lock lock(pluginsMutex_);
        auto libIt = libraries_.find(libraryKey);
        if (libIt != libraries_.end())
        {
            libIt->second.plugins.erase(pluginId);

            // 如果库中没有其他插件，卸载库
            if (libIt->second.plugins.empty())
            {
                libIt->second.loader->Unload();
                libraries_.erase(libIt);
            }
        }
    }

    NotifyPluginEvent(pluginId, PluginEvent::UNLOADED);

    return true;
}

std::shared_ptr<IPlugin> PluginManagerImpl::GetPlugin(const std::string &pluginId) const
{
    std::shared_lock lock(pluginsMutex_);
    auto it = plugins_.find(pluginId);
    return it != plugins_.end() ? it->second.plugin : nullptr;
}

std::vector<PluginInfo> PluginManagerImpl::GetAllPluginInfo() const
{
    std::vector<PluginInfo> result;
    std::shared_lock lock(pluginsMutex_);

    result.reserve(plugins_.size());
    for (const auto &pair : plugins_)
    {
        result.push_back(pair.second.info);
    }

    return result;
}

std::vector<std::string> PluginManagerImpl::GetLoadedPlugins() const
{
    std::vector<std::string> result;
    std::shared_lock lock(pluginsMutex_);

    result.reserve(plugins_.size());
    for (const auto &pair : plugins_)
    {
        result.push_back(pair.first);
    }

    return result;
}

int PluginManagerImpl::ScanPlugins(const std::string &pluginDir)
{
    if (!initialized_.load())
    {
        return 0;
    }

    if (!fs::exists(pluginDir) || !fs::is_directory(pluginDir))
    {
        std::cerr << "Plugin directory does not exist: " << pluginDir << std::endl;
        return 0;
    }

    int loadedCount = 0;
    std::vector<std::string> pluginPaths;

    try
    {
        // 先收集所有可能的插件文件
        for (const auto &entry : fs::directory_iterator(pluginDir))
        {
            if (entry.is_regular_file())
            {
                std::string path = entry.path().string();
                std::string ext = entry.path().extension().string();

                // 检查是否为动态库
#ifdef _WIN32
                if (ext == ".dll" || ext == ".DLL")
#elif __APPLE__
                if (ext == ".dylib" || ext == ".so" || ext == ".bundle")
#else
                if (ext == ".so")
#endif
                {
                    pluginPaths.push_back(path);
                }
            }
        }

        // 按文件名排序，保证加载顺序可预测
        std::sort(pluginPaths.begin(), pluginPaths.end());

        // 加载插件
        for (const auto& path : pluginPaths)
        {
            std::string pluginId = LoadPlugin(path);
            if (!pluginId.empty())
            {
                loadedCount++;
                std::cout << "Loaded plugin: " << pluginId << " from " << path << std::endl;
            }
        }
    }
    catch (const fs::filesystem_error &e)
    {
        std::cerr << "Error scanning plugin directory: " << e.what() << std::endl;
    }

    return loadedCount;
}

bool PluginManagerImpl::StartAllPlugins()
{
    if (!initialized_.load())
    {
        return false;
    }

    std::vector<std::string> failedPlugins;

    // 先检查依赖
    std::shared_lock lock(pluginsMutex_);
    for (auto &pair : plugins_)
    {
        if (!CheckDependencies(pair.first))
        {
            std::cerr << "Plugin " << pair.first << " has unsatisfied dependencies" << std::endl;
            failedPlugins.push_back(pair.first);
            continue;
        }
    }

    // 如果依赖检查失败，返回false
    if (!failedPlugins.empty())
    {
        return false;
    }

    failedPlugins.clear();

    // 启动插件
    for (auto &pair : plugins_)
    {
        if (!pair.second.info.isEnabled)
        {
            continue;
        }

        try
        {
            if (!pair.second.plugin->Start())
            {
                failedPlugins.push_back(pair.first);
                std::cerr << "Failed to start plugin: " << pair.first << std::endl;
            }
            else
            {
                pair.second.info.isEnabled = true;
                NotifyPluginEvent(pair.first, PluginEvent::STARTED);
            }
        }
        catch (const std::exception& e)
        {
            failedPlugins.push_back(pair.first);
            std::cerr << "Exception starting plugin " << pair.first << ": " << e.what() << std::endl;
        }
    }

    return failedPlugins.empty();
}

void PluginManagerImpl::StopAllPlugins()
{
    if (!initialized_.load())
    {
        return;
    }

    // 反向停止插件（依赖顺序）
    std::vector<std::string> pluginIds;
    {
        std::shared_lock lock(pluginsMutex_);
        pluginIds.reserve(plugins_.size());
        for (const auto &pair : plugins_)
        {
            pluginIds.push_back(pair.first);
        }
    }

    // 反向遍历，先停止依赖项
    for (auto it = pluginIds.rbegin(); it != pluginIds.rend(); ++it)
    {
        StopPlugin(*it);
    }
}

bool PluginManagerImpl::StartPlugin(const std::string &pluginId)
{
    if (!initialized_.load())
    {
        return false;
    }

    // 检查依赖
    if (!CheckDependencies(pluginId))
    {
        std::cerr << "Plugin " << pluginId << " has unsatisfied dependencies" << std::endl;
        return false;
    }

    std::shared_ptr<IPlugin> plugin;
    {
        std::shared_lock lock(pluginsMutex_);
        auto it = plugins_.find(pluginId);
        if (it == plugins_.end())
        {
            return false;
        }
        plugin = it->second.plugin;
    }

    try
    {
        if (plugin->Start())
        {
            {
                std::unique_lock lock(pluginsMutex_);
                plugins_[pluginId].info.isEnabled = true;
            }
            NotifyPluginEvent(pluginId, PluginEvent::STARTED);
            return true;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception starting plugin " << pluginId << ": " << e.what() << std::endl;
    }

    return false;
}

void PluginManagerImpl::StopPlugin(const std::string &pluginId)
{
    if (!initialized_.load())
    {
        return;
    }

    std::shared_ptr<IPlugin> plugin;
    {
        std::shared_lock lock(pluginsMutex_);
        auto it = plugins_.find(pluginId);
        if (it == plugins_.end())
        {
            return;
        }
        plugin = it->second.plugin;
    }

    try
    {
        plugin->Stop();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception stopping plugin " << pluginId << ": " << e.what() << std::endl;
    }

    {
        std::unique_lock lock(pluginsMutex_);
        plugins_[pluginId].info.isEnabled = false;
    }

    NotifyPluginEvent(pluginId, PluginEvent::STOPPED);
}

bool PluginManagerImpl::RegisterPluginFactory(const std::string &pluginType,
                                              std::function<std::shared_ptr<IPlugin>()> factory)
{
    if (pluginType.empty() || !factory)
    {
        return false;
    }

    std::unique_lock lock(pluginsMutex_);
    factories_[pluginType] = factory;
    return true;
}

void PluginManagerImpl::SetPluginEventCallback(PluginEventCallback callback)
{
    eventCallback_ = callback;
}

std::map<std::string, std::any> PluginManagerImpl::GetStatus() const
{
    std::map<std::string, std::any> status;

    std::shared_lock lock(pluginsMutex_);
    status["plugin_count"] = static_cast<int>(plugins_.size());
    status["library_count"] = static_cast<int>(libraries_.size());
    status["factory_count"] = static_cast<int>(factories_.size());
    status["initialized"] = initialized_.load();

    std::vector<std::string> loadedPlugins;
    std::vector<std::string> enabledPlugins;
    std::vector<std::string> disabledPlugins;

    for (const auto &pair : plugins_)
    {
        loadedPlugins.push_back(pair.first);
        if (pair.second.info.isEnabled)
        {
            enabledPlugins.push_back(pair.first);
        }
        else
        {
            disabledPlugins.push_back(pair.first);
        }
    }

    status["loaded_plugins"] = loadedPlugins;
    status["enabled_plugins"] = enabledPlugins;
    status["disabled_plugins"] = disabledPlugins;

    if (!configDir_.empty())
    {
        status["config_dir"] = configDir_;
    }

    return status;
}

bool PluginManagerImpl::CheckDependencies(const std::string &pluginId) const
{
    std::shared_lock lock(pluginsMutex_);
    auto it = plugins_.find(pluginId);
    if (it == plugins_.end())
    {
        return false;
    }

    const auto &dependencies = it->second.info.dependencies;
    for (const auto &dep : dependencies)
    {
        auto depIt = plugins_.find(dep);
        if (depIt == plugins_.end())
        {
            std::cerr << "Dependency not found: " << dep << " for plugin " << pluginId << std::endl;
            return false;
        }

        // 检查依赖插件是否已启用
        if (!depIt->second.info.isEnabled)
        {
            std::cerr << "Dependency not enabled: " << dep << " for plugin " << pluginId << std::endl;
            return false;
        }
    }

    return true;
}

bool PluginManagerImpl::ReloadPluginConfig(const std::string &pluginId)
{
    if (!initialized_.load())
    {
        return false;
    }

    {
        std::shared_lock lock(pluginsMutex_);
        auto it = plugins_.find(pluginId);
        if (it == plugins_.end())
        {
            return false;
        }
    }

    LoadPluginConfig(pluginId);
    return true;
}

void PluginManagerImpl::LoadPluginConfig(const std::string &pluginId)
{
    if (configDir_.empty())
    {
        return;
    }

    std::string configFile = configDir_ + "/" + pluginId + ".json";
    if (!fs::exists(configFile))
    {
        // 创建默认配置文件
        SavePluginConfig(pluginId);
        return;
    }

    try
    {
        std::ifstream file(configFile);
        if (!file.is_open())
        {
            std::cerr << "Failed to open config file: " << configFile << std::endl;
            return;
        }

        // 简单的JSON解析（实际项目应使用JSON库如nlohmann/json）
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

        std::unique_lock lock(pluginsMutex_);
        auto& config = plugins_[pluginId].config;
        config["raw_content"] = content;

        // 这里可以添加更复杂的JSON解析
        // 示例：解析简单的键值对
        std::map<std::string, std::any> parsedConfig;
        
        // 移除空白字符
        std::string clean;
        for (char c : content)
        {
            if (!std::isspace(c) && c != '{' && c != '}' && c != '"')
            {
                clean += c;
            }
        }

        // 解析逗号分隔的键值对
        std::stringstream ss(clean);
        std::string pair;
        while (std::getline(ss, pair, ','))
        {
            auto colonPos = pair.find(':');
            if (colonPos != std::string::npos)
            {
                std::string key = pair.substr(0, colonPos);
                std::string value = pair.substr(colonPos + 1);
                
                // 尝试解析为数字或布尔值
                try
                {
                    if (value == "true" || value == "false")
                    {
                        parsedConfig[key] = (value == "true");
                    }
                    else if (value.find('.') != std::string::npos)
                    {
                        parsedConfig[key] = std::stod(value);
                    }
                    else
                    {
                        parsedConfig[key] = std::stoi(value);
                    }
                }
                catch (...)
                {
                    // 默认为字符串
                    parsedConfig[key] = value;
                }
            }
        }

        config["parsed"] = parsedConfig;

        std::cout << "Loaded config for plugin: " << pluginId << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error loading plugin config: " << e.what() << std::endl;
    }
}

void PluginManagerImpl::SavePluginConfig(const std::string &pluginId)
{
    if (configDir_.empty())
    {
        return;
    }

    std::string configFile = configDir_ + "/" + pluginId + ".json";

    try
    {
        // 确保配置目录存在
        fs::create_directories(fs::path(configFile).parent_path());

        std::ofstream file(configFile);
        if (!file.is_open())
        {
            std::cerr << "Failed to open config file for writing: " << configFile << std::endl;
            return;
        }

        // 创建默认配置
        std::string defaultConfig = R"({
    "enabled": true,
    "auto_start": false,
    "settings": {
    }
})";

        file << defaultConfig;
        file.close();

        std::cout << "Created default config for plugin: " << pluginId << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error saving plugin config: " << e.what() << std::endl;
    }
}

void PluginManagerImpl::NotifyPluginEvent(const std::string &pluginId,
                                          PluginEvent event,
                                          const std::any &data)
{
    if (!eventCallback_)
    {
        return;
    }

    PluginEventData eventData;
    eventData.pluginId = pluginId;
    eventData.event = event;
    eventData.data = data;
    eventData.timestamp = duration_cast<milliseconds>(
                              system_clock::now().time_since_epoch())
                              .count();

    try
    {
        eventCallback_(eventData);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error in plugin event callback: " << e.what() << std::endl;
    }
}

void PluginManagerImpl::CleanupExpiredPlugins()
{
    std::unique_lock lock(pluginsMutex_);

    auto now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    const int64_t EXPIRATION_TIME = 3600000; // 1小时

    // 这里可以实现插件过期清理逻辑
    // 例如：清理长时间未使用的插件
    std::vector<std::string> expiredPlugins;

    for (const auto& pair : plugins_)
    {
        // 如果插件已停止且长时间未使用，可以考虑卸载
        if (!pair.second.info.isEnabled)
        {
            // 获取插件最后使用时间（需要插件接口支持）
            // 这里简化处理，实际需要从插件获取
            expiredPlugins.push_back(pair.first);
        }
    }

    lock.unlock();

    for (const auto& pluginId : expiredPlugins)
    {
        // 可以选择卸载长时间未使用的插件
        // UnloadPlugin(pluginId);
    }
}

// =============== 工厂函数 ===============

std::shared_ptr<IPluginManager> CreatePluginManager()
{
    return std::make_shared<PluginManagerImpl>();
}

} // namespace PluginSystem