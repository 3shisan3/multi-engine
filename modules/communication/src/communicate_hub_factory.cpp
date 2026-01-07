#include "communicate_hub_factory.h"
#include "multi_protocol_communicate.h"

// 包含所有协议适配器头文件
#ifdef USE_ASIO
#include "protocol/tcp_adapter.h"
#include "protocol/udp_adapter.h"
#endif

#ifdef USE_DDS
#include "protocol/dds_adapter.h"
#endif

#ifdef USE_WEBSOCKET
#include "protocol/websocket_adapter.h"
#endif

namespace CommunicationModule
{

// 静态成员初始化
std::string CommunicationHubFactory::defaultProtocolAdapter_ = "TCP";
std::mutex CommunicationHubFactory::factoryMutex_;

std::map<std::string, std::function<std::shared_ptr<IProtocolAdapter>()>> &
CommunicationHubFactory::GetFactoryMap()
{
    static std::map<std::string, std::function<std::shared_ptr<IProtocolAdapter>()>> factoryMap;
    static bool initialized = false;

    if (!initialized)
    {
        // 注册TCP适配器
        factoryMap["TCP"] = []() -> std::shared_ptr<IProtocolAdapter> {
            return std::make_shared<TCPProtocolAdapter>();
        };

        // 注册UDP适配器
        factoryMap["UDP"] = []() -> std::shared_ptr<IProtocolAdapter> {
            return std::make_shared<UDPProtocolAdapter>();
        };

        // 注册DDS适配器（如果启用）
#ifdef USE_DDS
        factoryMap["DDS"] = []() -> std::shared_ptr<IProtocolAdapter> {
            return std::make_shared<DDSProtocolAdapter>();
        };
#endif

        // 注册WebSocket适配器（如果启用）
#ifdef USE_WEBSOCKET
        factoryMap["WebSocket"] = []() -> std::shared_ptr<IProtocolAdapter> {
            return std::make_shared<WebSocketProtocolAdapter>();
        };
#endif

        // 注册MQTT适配器（预留）
        factoryMap["MQTT"] = []() -> std::shared_ptr<IProtocolAdapter> {
            // 返回nullptr，需要时再实现
            return nullptr;
        };

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

    // 创建默认适配器
    if (!defaultProtocolAdapter_.empty())
    {
        auto defaultIt = factoryMap.find(defaultProtocolAdapter_);
        if (defaultIt != factoryMap.end())
        {
            return defaultIt->second();
        }
    }

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
        // 检查适配器是否可用（可能返回nullptr）
        auto factory = factoryMap[protocolType];
        if (factory)
        {
            auto adapter = factory();
            return adapter != nullptr;
        }
    }

    return false;
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