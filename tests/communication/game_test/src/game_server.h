#pragma once

#include "CommunicationHubFactory.h"
#include "InternalCommunicateHub.h"
#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>

// 演示：分布式游戏服务器使用通信模块的场景
// 本示例展示如何在实际业务场景中应用通信枢纽模块

namespace GameDemo {

// 使用nlohmann/json库
using json = nlohmann::json;

/**
 * @brief 游戏服务器状态
 * @note 展示如何管理服务器生命周期
 */
enum class ServerState {
    INITIALIZING,
    RUNNING,
    SHUTTING_DOWN,
    STOPPED
};

/**
 * @brief 游戏区域类型
 * @note 展示分布式环境中区域管理
 */
enum class ZoneType {
    MAIN_CITY = 0,     // 主城
    DUNGEON,           // 副本
    BATTLEFIELD,       // 战场
    WILDERNESS,        // 野外
    INSTANCE           // 实例
};

/**
 * @brief 玩家会话信息
 * @note 展示如何使用ConnectionContext管理玩家连接
 */
struct PlayerInfo {
    uint64_t playerId;
    std::string account;
    std::string name;
    ZoneType currentZone;
    double positionX;
    double positionY;
    double positionZ;
    int64_t lastActivityTime;
    uint64_t sessionId;  // 对应通信枢纽中的ClientId
};

/**
 * @brief 游戏服务器演示类
 * @note 本类展示如何在实际业务中：
 * 1. 使用ICommunicationHub管理多协议通信
 * 2. 使用InternalCommunicateHub进行模块间通信
 * 3. 处理玩家连接和消息路由
 * 4. 实现游戏业务逻辑
 */
class GameServer {
public:
    explicit GameServer(const std::string& configFile);
    ~GameServer();
    
    // 生命周期管理
    bool Initialize();
    bool Start();
    void Stop();
    void Shutdown();
    
    // 服务器状态查询
    ServerState GetState() const { return state_; }
    size_t GetPlayerCount() const;
    size_t GetZonePlayerCount(ZoneType zone) const;
    
    // 玩家管理（演示业务逻辑）
    bool PlayerLogin(uint64_t playerId, const std::string& account, 
                     const std::string& name, uint64_t sessionId);
    bool PlayerLogout(uint64_t playerId);
    bool UpdatePlayerPosition(uint64_t playerId, double x, double y, double z);
    bool ChangePlayerZone(uint64_t playerId, ZoneType newZone);
    
    // 消息发送（演示通信模块使用）
    bool SendToPlayer(uint64_t playerId, 
                      CommunicationModule::MessageType type,
                      const std::vector<uint8_t>& data,
                      const std::string& topic = "");
    bool BroadcastToZone(ZoneType zone,
                        CommunicationModule::MessageType type,
                        const std::vector<uint8_t>& data,
                        const std::string& topic = "");
    
    // 获取统计信息（演示监控功能）
    json GetStatistics() const;
    
private:
    // 初始化方法
    bool LoadConfig(const std::string& configFile);
    bool InitializeCommunicationHub();  // 演示通信枢纽初始化
    bool InitializeGameWorld();
    
    // 内部处理方法
    std::string GetZoneName(ZoneType zone) const;
    void BroadcastPlayerEnterZone(ZoneType zone, uint64_t playerId, 
                                 const std::string& playerName);
    void BroadcastPlayerLeaveZone(ZoneType zone, uint64_t playerId,
                                 const std::string& playerName);
    
    // 消息构建方法（演示nlohmann/json使用）
    std::vector<uint8_t> CreateWelcomeMessage(const PlayerInfo& player);
    std::vector<uint8_t> CreateZoneChangeMessage(ZoneType newZone);
    std::vector<uint8_t> CreatePositionUpdateMessage(uint64_t playerId,
                                                    double x, double y, double z);
    
    // 成员变量
    std::atomic<ServerState> state_{ServerState::STOPPED};
    std::string serverId_;
    int serverPort_;
    int maxPlayers_;
    
    // 通信模块实例（核心组件）
    std::shared_ptr<CommunicationModule::ICommunicationHub> commHub_;
    CommunicationModule::InternalCommunicateHub* internalHub_;

    // 消息处理器
    std::shared_ptr<class GameMessageHandler> messageHandler_;
    
    // 游戏数据（业务数据）
    mutable std::mutex playersMutex_;
    std::unordered_map<uint64_t, PlayerInfo> players_;
    std::unordered_map<ZoneType, std::unordered_set<uint64_t>> zonePlayers_;
    
    // 统计信息
    struct Statistics {
        std::atomic<uint64_t> totalLogins{0};
        std::atomic<uint64_t> totalLogouts{0};
        std::atomic<uint64_t> messagesSent{0};
        std::atomic<uint64_t> messagesReceived{0};
        std::atomic<uint64_t> zoneChanges{0};
        std::atomic<uint64_t> communicationErrors{0};
    } stats_;
};

} // namespace GameDemo