#pragma once

#include "game_server.h"
#include "CommunicationHub.h"
#include "InternalCommunicateHub.h"
#include <nlohmann/json.hpp>
#include <memory>

namespace GameDemo {

// 使用nlohmann/json库
using json = nlohmann::json;

/**
 * @brief 游戏消息处理器
 * @note 演示如何：
 * 1. 实现IDataReceiver处理网络消息
 * 2. 实现IInternalEventHandler处理内部事件
 * 3. 使用json解析和构建消息
 * 4. 将消息路由到业务逻辑
 */
class GameMessageHandler : public CommunicationModule::IDataReceiver,
                           public CommunicationModule::IInternalEventHandler {
public:
    explicit GameMessageHandler(GameServer* server);
    
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
    
    GameServer* server_;  // 非拥有指针
};

} // namespace GameDemo