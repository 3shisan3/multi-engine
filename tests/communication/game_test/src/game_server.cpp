#include "game_server.h"
#include "game_message_handler.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>

using namespace std::chrono;

namespace GameDemo {

GameServer::GameServer(const std::string& configFile) 
    : serverId_("GameDemoServer") {
    std::cout << "【通信模块应用演示】创建游戏服务器实例" << std::endl;
    
    if (!LoadConfig(configFile)) {
        throw std::runtime_error("配置加载失败: " + configFile);
    }
}

GameServer::~GameServer() {
    Shutdown();
}

bool GameServer::LoadConfig(const std::string& configFile) {
    try {
        std::ifstream file(configFile);
        if (!file.is_open()) {
            std::cerr << "无法打开配置文件: " << configFile << std::endl;
            return false;
        }
        
        json config;
        file >> config;
        
        // 读取基本配置
        serverId_ = config.value("server_id", "GameDemoServer");
        serverPort_ = config.value("server_port", 9000);
        maxPlayers_ = config.value("max_players", 1000);
        
        std::cout << "【配置加载】服务器ID: " << serverId_ 
                  << ", 端口: " << serverPort_ 
                  << ", 最大玩家: " << maxPlayers_ << std::endl;
        
        return true;
        
    } catch (const json::exception& e) {
        std::cerr << "JSON解析错误: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "配置加载错误: " << e.what() << std::endl;
        return false;
    }
}

bool GameServer::Initialize() {
    if (state_ != ServerState::STOPPED) {
        std::cerr << "服务器未处于停止状态" << std::endl;
        return false;
    }
    
    state_ = ServerState::INITIALIZING;
    
    std::cout << "【初始化】开始初始化游戏服务器..." << std::endl;
    
    try {
        // 1. 初始化通信枢纽（核心步骤）
        if (!InitializeCommunicationHub()) {
            std::cerr << "通信枢纽初始化失败" << std::endl;
            return false;
        }
        
        // 2. 初始化游戏世界
        if (!InitializeGameWorld()) {
            std::cerr << "游戏世界初始化失败" << std::endl;
            return false;
        }
        
        std::cout << "【初始化】游戏服务器初始化成功" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "初始化错误: " << e.what() << std::endl;
        state_ = ServerState::STOPPED;
        return false;
    }
}

bool GameServer::InitializeCommunicationHub() {
    std::cout << "【通信模块】开始初始化通信枢纽..." << std::endl;
    
    try {
        // 1. 创建多协议通信枢纽实例
        commHub_ = CommunicationModule::CommunicationHubFactory::CreateHub();
        if (!commHub_) {
            std::cerr << "创建通信枢纽失败" << std::endl;
            return false;
        }
        
        // 2. 配置通信协议
        std::vector<CommunicationModule::CommunicationConfig> configs;
        
        // TCP协议配置（可靠通信）
        CommunicationModule::CommunicationConfig tcpConfig;
        tcpConfig.protocol = "TCP";
        tcpConfig.serverAddress = "0.0.0.0";
        tcpConfig.serverPort = serverPort_;
        tcpConfig.heartbeatIntervalMs = 1000;
        tcpConfig.connectionTimeoutMs = 30000;
        tcpConfig.maxClients = maxPlayers_;
        tcpConfig.enableEncryption = false;
        
        tcpConfig.protocolParams["keep_alive"] = "true";
        tcpConfig.protocolParams["tcp_nodelay"] = "true";
        tcpConfig.protocolParams["io_threads"] = "2";
        
        configs.push_back(tcpConfig);
        
        // UDP协议配置（实时通信）
        CommunicationModule::CommunicationConfig udpConfig;
        udpConfig.protocol = "UDP";
        udpConfig.serverAddress = "0.0.0.0";
        udpConfig.serverPort = serverPort_ + 1;
        udpConfig.maxClients = maxPlayers_;
        
        udpConfig.protocolParams["enable_broadcast"] = "true";
        udpConfig.protocolParams["receive_buffer_size"] = "65507";
        
        configs.push_back(udpConfig);
        
        // 3. 初始化通信枢纽
        if (!commHub_->Initialize(configs)) {
            std::cerr << "通信枢纽初始化失败" << std::endl;
            return false;
        }
        
        // 4. 设置错误回调
        commHub_->SetErrorCallback([this](const std::string& source, 
                                          const std::string& error) {
            std::cerr << "【通信错误】[" << source << "] " << error << std::endl;
            stats_.communicationErrors++;
        });
        
        // 5. 初始化内部通信枢纽
        internalHub_ = &SingletonTemplate<CommunicationModule::InternalCommunicateHub>::getSingletonInstance();
        
        std::map<std::string, std::any> internalConfig = {
            {"worker_threads", 4}
        };
        
        if (!internalHub_->Initialize(internalConfig)) {
            std::cerr << "内部通信枢纽初始化失败" << std::endl;
            return false;
        }
        
        // 6. 创建并注册消息处理器
        messageHandler_ = std::make_shared<GameMessageHandler>(this);
        
        if (!commHub_->RegisterDataReceiver(std::dynamic_pointer_cast<CommunicationModule::IDataReceiver>(messageHandler_))) {
            std::cerr << "注册消息处理器失败" << std::endl;
            return false;
        }
        
        if (!internalHub_->RegisterEventHandler(std::dynamic_pointer_cast<CommunicationModule::IInternalEventHandler>(messageHandler_))) {
            std::cerr << "注册内部事件处理器失败" << std::endl;
            return false;
        }
        
        std::cout << "【通信模块】通信枢纽初始化成功" << std::endl;
        std::cout << "  - TCP协议: 端口 " << serverPort_ << std::endl;
        std::cout << "  - UDP协议: 端口 " << (serverPort_ + 1) << std::endl;
        std::cout << "  - 最大连接数: " << maxPlayers_ << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "通信枢纽初始化错误: " << e.what() << std::endl;
        return false;
    }
}

bool GameServer::InitializeGameWorld() {
    std::cout << "【游戏世界】初始化游戏世界..." << std::endl;
    
    {
        std::lock_guard<std::mutex> lock(playersMutex_);
        players_.clear();
        zonePlayers_.clear();
        
        // 初始化所有区域
        for (int i = 0; i < static_cast<int>(ZoneType::INSTANCE) + 1; ++i) {
            zonePlayers_[static_cast<ZoneType>(i)] = {};
        }
    }
    
    std::cout << "【游戏世界】游戏世界初始化完成" << std::endl;
    return true;
}

bool GameServer::Start() {
    if (state_ != ServerState::INITIALIZING) {
        std::cerr << "服务器未初始化完成" << std::endl;
        return false;
    }
    
    try {
        std::cout << "【启动】启动游戏服务器..." << std::endl;
        
        // 1. 启动通信枢纽
        if (!commHub_->Start()) {
            std::cerr << "启动通信枢纽失败" << std::endl;
            return false;
        }
        
        // 2. 启动内部通信枢纽
        if (!internalHub_->Start()) {
            std::cerr << "启动内部通信枢纽失败" << std::endl;
            return false;
        }
        
        state_ = ServerState::RUNNING;
        
        std::cout << "========================================" << std::endl;
        std::cout << " 游戏服务器启动成功" << std::endl;
        std::cout << " 服务器ID: " << serverId_ << std::endl;
        std::cout << " 通信协议: TCP(" << serverPort_ << ") UDP(" 
                  << (serverPort_ + 1) << ")" << std::endl;
        std::cout << " 最大玩家: " << maxPlayers_ << std::endl;
        std::cout << "========================================" << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "启动错误: " << e.what() << std::endl;
        state_ = ServerState::STOPPED;
        return false;
    }
}

void GameServer::Stop() {
    if (state_ != ServerState::RUNNING) {
        return;
    }
    
    state_ = ServerState::SHUTTING_DOWN;
    
    std::cout << "【停止】停止游戏服务器..." << std::endl;
    
    // 断开所有玩家连接
    {
        std::lock_guard<std::mutex> lock(playersMutex_);
        std::cout << "断开 " << players_.size() << " 个玩家连接" << std::endl;
        players_.clear();
        zonePlayers_.clear();
    }
    
    // 停止通信枢纽
    if (commHub_) {
        commHub_->Stop();
    }
    
    if (internalHub_) {
        internalHub_->Stop();
    }
    
    state_ = ServerState::STOPPED;
    std::cout << "【停止】游戏服务器已停止" << std::endl;
}

void GameServer::Shutdown() {
    Stop();
    
    if (commHub_) {
        commHub_->Shutdown();
        commHub_.reset();
    }
    
    if (internalHub_) {
        internalHub_->Shutdown();
    }
    
    std::cout << "【关闭】游戏服务器关闭完成" << std::endl;
}

bool GameServer::PlayerLogin(uint64_t playerId, const std::string& account,
                            const std::string& name, uint64_t sessionId) {
    if (state_ != ServerState::RUNNING) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(playersMutex_);
    
    // 检查玩家是否已登录
    if (players_.find(playerId) != players_.end()) {
        std::cerr << "玩家 " << playerId << " 已登录" << std::endl;
        return false;
    }
    
    // 创建玩家信息
    PlayerInfo player;
    player.playerId = playerId;
    player.account = account;
    player.name = name;
    player.currentZone = ZoneType::MAIN_CITY;
    player.positionX = 100.0;
    player.positionY = 0.0;
    player.positionZ = 100.0;
    player.lastActivityTime = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    player.sessionId = sessionId;
    
    // 添加到玩家列表
    players_[playerId] = player;
    zonePlayers_[player.currentZone].insert(playerId);
    
    stats_.totalLogins++;
    
    std::cout << "【玩家登录】" << name << " (ID: " << playerId 
              << ", 区域: " << GetZoneName(player.currentZone) << ")" << std::endl;
    
    // 使用nlohmann/json构建欢迎消息
    std::vector<uint8_t> welcomeMsg = CreateWelcomeMessage(player);
    
    // 发送登录成功消息（使用TCP协议，可靠传输）
    if (SendToPlayer(playerId, CommunicationModule::MessageType::EVENT, 
                    welcomeMsg, "player_login")) {
        std::cout << "  发送欢迎消息成功" << std::endl;
    }
    
    // 通知区域内的其他玩家
    BroadcastPlayerEnterZone(player.currentZone, playerId, player.name);
    
    return true;
}

bool GameServer::PlayerLogout(uint64_t playerId) {
    std::lock_guard<std::mutex> lock(playersMutex_);
    
    auto it = players_.find(playerId);
    if (it == players_.end()) {
        return false;
    }
    
    const PlayerInfo& player = it->second;
    
    // 从区域中移除
    zonePlayers_[player.currentZone].erase(playerId);
    
    // 通知区域内的其他玩家
    BroadcastPlayerLeaveZone(player.currentZone, playerId, player.name);
    
    // 从玩家列表中移除
    players_.erase(it);
    
    stats_.totalLogouts++;
    
    std::cout << "【玩家登出】" << player.name << " (ID: " << playerId << ")" << std::endl;
    
    return true;
}

bool GameServer::UpdatePlayerPosition(uint64_t playerId, double x, double y, double z) {
    std::lock_guard<std::mutex> lock(playersMutex_);
    
    auto it = players_.find(playerId);
    if (it == players_.end()) {
        return false;
    }
    
    // 更新位置
    it->second.positionX = x;
    it->second.positionY = y;
    it->second.positionZ = z;
    it->second.lastActivityTime = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    
    // 构建位置更新消息
    std::vector<uint8_t> positionMsg = CreatePositionUpdateMessage(playerId, x, y, z);
    
    // 广播位置更新给同区域玩家（使用UDP协议，低延迟）
    CommunicationModule::NetworkMessage udpMsg;
    udpMsg.type = CommunicationModule::MessageType::DATA;
    udpMsg.sourceClientId = playerId;
    udpMsg.topic = "player_position";
    udpMsg.payload = positionMsg;
    udpMsg.timestamp = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    
    // 使用UDP协议发送
    commHub_->PublishToTopic("player_position", udpMsg);
    
    std::cout << "【位置更新】玩家 " << playerId << " 位置: (" 
              << x << ", " << y << ", " << z << ")" << std::endl;
    
    return true;
}

bool GameServer::ChangePlayerZone(uint64_t playerId, ZoneType newZone) {
    std::lock_guard<std::mutex> lock(playersMutex_);
    
    auto it = players_.find(playerId);
    if (it == players_.end()) {
        return false;
    }
    
    PlayerInfo& player = it->second;
    ZoneType oldZone = player.currentZone;
    
    if (oldZone == newZone) {
        return true;
    }
    
    // 从旧区域移除
    zonePlayers_[oldZone].erase(playerId);
    
    // 添加到新区域
    player.currentZone = newZone;
    zonePlayers_[newZone].insert(playerId);
    
    stats_.zoneChanges++;
    
    // 构建区域切换消息（使用nlohmann/json）
    std::vector<uint8_t> zoneChangeMsg = CreateZoneChangeMessage(newZone);
    
    // 发送区域切换消息给玩家（使用TCP协议）
    SendToPlayer(playerId, CommunicationModule::MessageType::EVENT, 
                zoneChangeMsg, "zone_change");
    
    // 广播给两个区域的玩家
    BroadcastPlayerLeaveZone(oldZone, playerId, player.name);
    BroadcastPlayerEnterZone(newZone, playerId, player.name);
    
    std::cout << "【区域切换】玩家 " << player.name << " 从 " 
              << GetZoneName(oldZone) << " 切换到 " 
              << GetZoneName(newZone) << std::endl;
    
    return true;
}

bool GameServer::SendToPlayer(uint64_t playerId, 
                             CommunicationModule::MessageType type,
                             const std::vector<uint8_t>& data,
                             const std::string& topic) {
    if (state_ != ServerState::RUNNING) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(playersMutex_);
    
    auto it = players_.find(playerId);
    if (it == players_.end()) {
        return false;
    }
    
    // 创建消息
    CommunicationModule::NetworkMessage message;
    message.type = type;
    message.targetClientId = it->second.sessionId;
    message.payload = data;
    message.topic = topic;
    message.timestamp = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    
    // 根据消息类型选择协议
    std::string protocol = "TCP";  // 默认使用TCP
    
    if (type == CommunicationModule::MessageType::HEARTBEAT ||
        topic == "player_position") {
        // 心跳和位置更新使用UDP
        protocol = "UDP";
    }
    
    bool success = commHub_->SendViaProtocol(protocol, 
                                            it->second.sessionId, 
                                            message);
    
    if (success) {
        stats_.messagesSent++;
        std::cout << "【消息发送】玩家 " << playerId << " 消息类型: " 
                  << topic << " 协议: " << protocol << std::endl;
    }
    
    return success;
}

bool GameServer::BroadcastToZone(ZoneType zone,
                                CommunicationModule::MessageType type,
                                const std::vector<uint8_t>& data,
                                const std::string& topic) {
    if (state_ != ServerState::RUNNING) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(playersMutex_);
    
    auto zoneIt = zonePlayers_.find(zone);
    if (zoneIt == zonePlayers_.end() || zoneIt->second.empty()) {
        return false;
    }
    
    // 创建消息
    CommunicationModule::NetworkMessage message;
    message.type = type;
    message.payload = data;
    message.topic = topic;
    message.timestamp = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    
    int successCount = 0;
    
    // 发送给区域内的每个玩家
    for (uint64_t playerId : zoneIt->second) {
        auto playerIt = players_.find(playerId);
        if (playerIt != players_.end()) {
            message.targetClientId = playerIt->second.sessionId;
            
            std::string protocol = "TCP";
            if (type == CommunicationModule::MessageType::HEARTBEAT ||
                topic == "player_position") {
                protocol = "UDP";
            }
            
            if (commHub_->SendViaProtocol(protocol, 
                                         playerIt->second.sessionId, 
                                         message)) {
                successCount++;
            }
        }
    }
    
    if (successCount > 0) {
        stats_.messagesSent += successCount;
        std::cout << "【区域广播】区域 " << GetZoneName(zone) 
                  << " 发送给 " << successCount << " 个玩家" << std::endl;
    }
    
    return successCount > 0;
}

// 使用nlohmann/json构建消息
std::vector<uint8_t> GameServer::CreateWelcomeMessage(const PlayerInfo& player) {
    json welcome;
    welcome["message"] = "欢迎来到游戏, " + player.name + "!";
    welcome["player_id"] = player.playerId;
    welcome["zone"] = static_cast<int>(player.currentZone);
    welcome["zone_name"] = GetZoneName(player.currentZone);
    welcome["position"] = {player.positionX, player.positionY, player.positionZ};
    welcome["timestamp"] = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    welcome["server_time"] = std::chrono::system_clock::now()
        .time_since_epoch().count();
    
    std::string jsonStr = welcome.dump();
    return std::vector<uint8_t>(jsonStr.begin(), jsonStr.end());
}

std::vector<uint8_t> GameServer::CreateZoneChangeMessage(ZoneType newZone) {
    json zoneChange;
    zoneChange["zone_id"] = static_cast<int>(newZone);
    zoneChange["zone_name"] = GetZoneName(newZone);
    zoneChange["timestamp"] = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    zoneChange["message"] = "你已进入" + GetZoneName(newZone);
    
    std::string jsonStr = zoneChange.dump();
    return std::vector<uint8_t>(jsonStr.begin(), jsonStr.end());
}

std::vector<uint8_t> GameServer::CreatePositionUpdateMessage(uint64_t playerId,
                                                            double x, double y, double z) {
    json position;
    position["player_id"] = playerId;
    position["x"] = x;
    position["y"] = y;
    position["z"] = z;
    position["timestamp"] = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    
    std::string jsonStr = position.dump();
    return std::vector<uint8_t>(jsonStr.begin(), jsonStr.end());
}

void GameServer::BroadcastPlayerEnterZone(ZoneType zone, uint64_t playerId, 
                                         const std::string& playerName) {
    json enterMsg;
    enterMsg["type"] = "player_enter";
    enterMsg["player_id"] = playerId;
    enterMsg["player_name"] = playerName;
    enterMsg["zone"] = GetZoneName(zone);
    enterMsg["timestamp"] = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    
    std::string jsonStr = enterMsg.dump();
    std::vector<uint8_t> data(jsonStr.begin(), jsonStr.end());
    
    BroadcastToZone(zone, CommunicationModule::MessageType::EVENT,
                   data, "player_enter");
}

void GameServer::BroadcastPlayerLeaveZone(ZoneType zone, uint64_t playerId,
                                         const std::string& playerName) {
    json leaveMsg;
    leaveMsg["type"] = "player_leave";
    leaveMsg["player_id"] = playerId;
    leaveMsg["player_name"] = playerName;
    leaveMsg["zone"] = GetZoneName(zone);
    leaveMsg["timestamp"] = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    
    std::string jsonStr = leaveMsg.dump();
    std::vector<uint8_t> data(jsonStr.begin(), jsonStr.end());
    
    BroadcastToZone(zone, CommunicationModule::MessageType::EVENT,
                   data, "player_leave");
}

std::string GameServer::GetZoneName(ZoneType zone) const {
    switch (zone) {
        case ZoneType::MAIN_CITY: return "主城";
        case ZoneType::DUNGEON: return "副本";
        case ZoneType::BATTLEFIELD: return "战场";
        case ZoneType::WILDERNESS: return "野外";
        case ZoneType::INSTANCE: return "实例";
        default: return "未知区域";
    }
}

size_t GameServer::GetPlayerCount() const {
    std::lock_guard<std::mutex> lock(playersMutex_);
    return players_.size();
}

size_t GameServer::GetZonePlayerCount(ZoneType zone) const {
    std::lock_guard<std::mutex> lock(playersMutex_);
    
    auto it = zonePlayers_.find(zone);
    if (it != zonePlayers_.end()) {
        return it->second.size();
    }
    return 0;
}

json GameServer::GetStatistics() const {
    json stats;
    
    // 服务器状态
    stats["server_state"] = static_cast<int>(state_.load());
    stats["server_id"] = serverId_;
    stats["server_port"] = serverPort_;
    
    // 玩家统计
    stats["player_count"] = GetPlayerCount();
    
    // 区域统计
    json zoneStats;
    for (int i = 0; i < static_cast<int>(ZoneType::INSTANCE) + 1; ++i) {
        ZoneType zone = static_cast<ZoneType>(i);
        zoneStats[GetZoneName(zone)] = GetZonePlayerCount(zone);
    }
    stats["zone_statistics"] = zoneStats;
    
    // 通信统计
    json commStats;
    commStats["total_logins"] = stats_.totalLogins.load();
    commStats["total_logouts"] = stats_.totalLogouts.load();
    commStats["messages_sent"] = stats_.messagesSent.load();
    commStats["messages_received"] = stats_.messagesReceived.load();
    commStats["zone_changes"] = stats_.zoneChanges.load();
    commStats["communication_errors"] = stats_.communicationErrors.load();
    
    stats["communication_statistics"] = commStats;
    
    // 如果通信枢纽已初始化，获取其统计信息
    if (commHub_) {
        auto hubStats = commHub_->GetStatistics();
        json hubStatsJson;
        for (const auto& [key, value] : hubStats) {
            hubStatsJson[key] = value;
        }
        stats["communication_hub_statistics"] = hubStatsJson;
    }
    
    // 时间戳
    stats["timestamp"] = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    
    return stats;
}

} // namespace GameDemo