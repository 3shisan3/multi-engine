#include "communicate_hub_factory.h"
#include "multi_protocol_communicate.h"

namespace CommunicationModule
{

// 静态成员初始化
std::string CommunicationHubFactory::defaultProtocolAdapter_ = "DDS";
std::mutex CommunicationHubFactory::factoryMutex_;

std::map<std::string, std::function<std::shared_ptr<IProtocolAdapter>()>> &
CommunicationHubFactory::GetFactoryMap()
{
    static std::map<std::string, std::function<std::shared_ptr<IProtocolAdapter>()>> factoryMap;
    static bool initialized = false;

    if (!initialized)
    {
        // 可以在这里注册默认的协议适配器
        // 实际项目中会根据编译选项动态注册
        initialized = true;
    }
    return factoryMap;
}

std::shared_ptr<ICommunicationHub> CommunicationHubFactory::CreateHub()
{
    return std::make_shared<MultiProtocolCommunicationHub>();
}

std::shared_ptr<ICommunicationHub> CommunicationHubFactory::CreateHub(
    std::function<std::shared_ptr<ICommunicationHub>()> factory)
{
    if (factory)
    {
        return factory();
    }
    return CreateHub();
}

std::shared_ptr<IProtocolAdapter> CommunicationHubFactory::CreateProtocolAdapter(const std::string &protocolType)
{
    std::lock_guard<std::mutex> lock(factoryMutex_);
    auto &factoryMap = GetFactoryMap();

    auto it = factoryMap.find(protocolType);
    if (it != factoryMap.end())
    {
        return it->second();
    }

    // 尝试创建默认适配器
    if (protocolType == "TCP")
    {
        // 需要包含TCP适配器头文件
        // return std::make_shared<TCPProtocolAdapter>();
    }
    // 其他默认适配器...

    return nullptr;
}

std::shared_ptr<IProtocolAdapter> CommunicationHubFactory::CreateProtocolAdapter(
    const std::string &protocolType,
    std::function<std::shared_ptr<IProtocolAdapter>()> factory)
{
    if (factory)
    {
        return factory();
    }
    return CreateProtocolAdapter(protocolType);
}

bool CommunicationHubFactory::RegisterProtocolAdapter(
    const std::string &protocolType,
    std::function<std::shared_ptr<IProtocolAdapter>()> factory)
{
    if (protocolType.empty() || !factory)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(factoryMutex_);
    auto &factoryMap = GetFactoryMap();

    if (factoryMap.find(protocolType) != factoryMap.end())
    {
        // 已存在，可以选择覆盖或返回false
        return false;
    }

    factoryMap[protocolType] = factory;
    return true;
}

bool CommunicationHubFactory::UnregisterProtocolAdapter(const std::string &protocolType)
{
    std::lock_guard<std::mutex> lock(factoryMutex_);
    auto &factoryMap = GetFactoryMap();

    auto it = factoryMap.find(protocolType);
    if (it == factoryMap.end())
    {
        return false;
    }

    factoryMap.erase(it);
    return true;
}

std::vector<std::string> CommunicationHubFactory::GetSupportedProtocolTypes()
{
    std::lock_guard<std::mutex> lock(factoryMutex_);
    auto &factoryMap = GetFactoryMap();

    std::vector<std::string> protocols;
    protocols.reserve(factoryMap.size());

    for (const auto &pair : factoryMap)
    {
        protocols.push_back(pair.first);
    }

    // 添加硬编码支持的协议
    if (factoryMap.find("TCP") == factoryMap.end())
    {
        protocols.push_back("TCP");
    }
    if (factoryMap.find("UDP") == factoryMap.end())
    {
        protocols.push_back("UDP");
    }
    if (factoryMap.find("WebSocket") == factoryMap.end())
    {
        protocols.push_back("WebSocket");
    }

    return protocols;
}

bool CommunicationHubFactory::IsProtocolSupported(const std::string &protocolType)
{
    std::lock_guard<std::mutex> lock(factoryMutex_);
    auto &factoryMap = GetFactoryMap();

    if (factoryMap.find(protocolType) != factoryMap.end())
    {
        return true;
    }

    // 检查硬编码支持
    return protocolType == "TCP" || protocolType == "UDP" || protocolType == "WebSocket";
}

void CommunicationHubFactory::SetDefaultProtocolAdapter(const std::string &protocolType)
{
    std::lock_guard<std::mutex> lock(factoryMutex_);
    defaultProtocolAdapter_ = protocolType;
}

std::string CommunicationHubFactory::GetDefaultProtocolAdapter()
{
    std::lock_guard<std::mutex> lock(factoryMutex_);
    return defaultProtocolAdapter_;
}

} // namespace CommunicationModule