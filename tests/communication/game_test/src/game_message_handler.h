#pragma once

#include "CommunicationHub.h"
#include "InternalCommunicateHub.h"
#include <nlohmann/json.hpp>
#include <memory>

// 前向声明，避免循环依赖
namespace GameDemo {
    class GameServer;
}

namespace GameDemo {

// 使用nlohmann/json库
using json = nlohmann::json;

/**
 * @brief 游戏消息处理器
 */
class GameMessageHandler : public CommunicationModule::IDataReceiver,
                           public CommunicationModule::IInternalEventHandler {
public:
    explicit GameMessageHandler(GameServer* server);
    virtual ~GameMessageHandler();
    
    // IDataReceiver接口实现
    void OnDataReceived(const CommunicationModule::ConnectionContext& context,
                       const CommunicationModule::NetworkMessage& message) override;
    
    std::vector<CommunicationModule::MessageType> GetInterestedMessageTypes() const override {
        return {
            CommunicationModule::MessageType::CONTROL,
            CommunicationModule::MessageType::DATA,
            CommunicationModule::MessageType::COMMAND,
            CommunicationModule::MessageType::EVENT
        };
    }
    
    std::vector<std::string> GetSubscribedTopics() const override {
        return {
            "player_login",
            "player_logout",
            "player_move",
            "player_chat",
            "combat_event",
            "quest_update",
            "heartbeat"
        };
    }
    
    std::string GetReceiverName() const override {
        return "GameMessageHandler";
    }
    
    // IInternalEventHandler接口实现
    void HandleInternalEvent(const std::string& eventType,
                            const std::any& data,
                            const std::map<std::string, std::any>& metadata) override;
    
    std::vector<std::string> GetInterestedEventTypes() const override {
        return {
            "player_state_update",
            "zone_state_change",
            "system_announcement"
        };
    }
    
    std::string GetHandlerName() const override {
        return "GameInternalEventHandler";
    }
    
    // 初始化方法
    bool Initialize() override {
        return true;
    }
    
    // 清理方法
    void Cleanup() override {}
    
private:
    // 网络消息处理函数
    void HandlePlayerLogin(const CommunicationModule::ConnectionContext& context,
                          const CommunicationModule::NetworkMessage& message);
    void HandlePlayerLogout(const CommunicationModule::ConnectionContext& context,
                           const CommunicationModule::NetworkMessage& message);
    void HandlePlayerMove(const CommunicationModule::ConnectionContext& context,
                         const CommunicationModule::NetworkMessage& message);
    void HandlePlayerChat(const CommunicationModule::ConnectionContext& context,
                         const CommunicationModule::NetworkMessage& message);
    void HandleHeartbeat(const CommunicationModule::ConnectionContext& context,
                        const CommunicationModule::NetworkMessage& message);
    
    // 内部事件处理函数
    void HandlePlayerStateUpdate(const std::any& data,
                                const std::map<std::string, std::any>& metadata);
    void HandleZoneStateChange(const std::any& data,
                              const std::map<std::string, std::any>& metadata);
    
    // 辅助函数
    bool ParseJsonMessage(const std::vector<uint8_t>& data, json& result);
    
    GameServer* server_;  // 非拥有指针，由GameServer管理生命周期
};

} // namespace GameDemo