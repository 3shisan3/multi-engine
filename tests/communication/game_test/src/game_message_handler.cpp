#include "game_message_handler.h"
#include <iostream>
#include <sstream>

namespace GameDemo {

GameMessageHandler::GameMessageHandler(GameServer* server)
    : server_(server) {
    if (!server_) {
        throw std::invalid_argument("GameServer指针不能为空");
    }
    
    std::cout << "【消息处理器】创建游戏消息处理器" << std::endl;
}

void GameMessageHandler::OnDataReceived(const CommunicationModule::ConnectionContext& context,
                                       const CommunicationModule::NetworkMessage& message) {
    if (!server_) {
        return;
    }
    
    std::cout << "【消息接收】客户端ID: " << context.clientId 
              << ", 消息类型: " << static_cast<int>(message.type)
              << ", 主题: " << (message.topic.empty() ? "无" : message.topic) 
              << ", 数据大小: " << message.payload.size() << "字节" << std::endl;
    
    try {
        // 根据主题路由消息
        std::string topic = message.topic;
        
        if (!topic.empty()) {
            if (topic == "player_login") {
                HandlePlayerLogin(context, message);
            } else if (topic == "player_logout") {
                HandlePlayerLogout(context, message);
            } else if (topic == "player_move") {
                HandlePlayerMove(context, message);
            } else if (topic == "player_chat") {
                HandlePlayerChat(context, message);
            } else if (topic == "heartbeat") {
                HandleHeartbeat(context, message);
            } else {
                std::cout << "【消息处理】未知主题: " << topic << std::endl;
            }
        } else {
            // 根据消息类型处理
            switch (message.type) {
                case CommunicationModule::MessageType::CONTROL:
                    std::cout << "【控制消息】客户端: " << context.clientId << std::endl;
                    break;
                case CommunicationModule::MessageType::DATA:
                    std::cout << "【数据消息】客户端: " << context.clientId << std::endl;
                    break;
                default:
                    break;
            }
        }
        
    } catch (const json::exception& e) {
        std::cerr << "【JSON错误】消息解析失败: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "【消息处理错误】: " << e.what() << std::endl;
    }
}

void GameMessageHandler::HandlePlayerLogin(const CommunicationModule::ConnectionContext& context,
                                          const CommunicationModule::NetworkMessage& message) {
    json loginData;
    if (!ParseJsonMessage(message.payload, loginData)) {
        std::cerr << "【登录消息】JSON解析失败" << std::endl;
        return;
    }
    
    // 验证必需字段
    if (!loginData.contains("player_id") || 
        !loginData.contains("account") || 
        !loginData.contains("name")) {
        std::cerr << "【登录消息】缺少必需字段" << std::endl;
        return;
    }
    
    uint64_t playerId = loginData["player_id"].get<uint64_t>();
    std::string account = loginData["account"].get<std::string>();
    std::string name = loginData["name"].get<std::string>();
    
    std::cout << "【处理登录】玩家: " << name << " (ID: " << playerId 
              << ", 账户: " << account << ")" << std::endl;
    
    // 调用服务器登录处理
    if (server_->PlayerLogin(playerId, account, name, context.clientId)) {
        std::cout << "【登录成功】玩家 " << name << " 登录成功" << std::endl;
    } else {
        std::cerr << "【登录失败】玩家 " << name << " 登录失败" << std::endl;
    }
}

void GameMessageHandler::HandlePlayerLogout(const CommunicationModule::ConnectionContext& context,
                                           const CommunicationModule::NetworkMessage& message) {
    json logoutData;
    if (!ParseJsonMessage(message.payload, logoutData)) {
        std::cerr << "【登出消息】JSON解析失败" << std::endl;
        return;
    }
    
    uint64_t playerId = logoutData.value("player_id", 0);
    std::string reason = logoutData.value("reason", "正常退出");
    
    if (playerId == 0) {
        // 如果没有提供player_id，使用context.clientId
        playerId = context.clientId;
    }
    
    std::cout << "【处理登出】玩家ID: " << playerId << ", 原因: " << reason << std::endl;
    
    if (server_->PlayerLogout(playerId)) {
        std::cout << "【登出成功】玩家 " << playerId << " 已登出" << std::endl;
    }
}

void GameMessageHandler::HandlePlayerMove(const CommunicationModule::ConnectionContext& context,
                                         const CommunicationModule::NetworkMessage& message) {
    json moveData;
    if (!ParseJsonMessage(message.payload, moveData)) {
        std::cerr << "【移动消息】JSON解析失败" << std::endl;
        return;
    }
    
    // 验证必需字段
    if (!moveData.contains("x") || 
        !moveData.contains("y") || 
        !moveData.contains("z")) {
        std::cerr << "【移动消息】缺少位置字段" << std::endl;
        return;
    }
    
    double x = moveData["x"].get<double>();
    double y = moveData["y"].get<double>();
    double z = moveData["z"].get<double>();
    
    // 使用context.clientId作为playerId
    uint64_t playerId = context.clientId;
    
    std::cout << "【处理移动】玩家 " << playerId << " 位置: (" 
              << x << ", " << y << ", " << z << ")" << std::endl;
    
    // 更新玩家位置
    if (server_->UpdatePlayerPosition(playerId, x, y, z)) {
        std::cout << "【位置更新成功】玩家 " << playerId << std::endl;
    }
}

void GameMessageHandler::HandlePlayerChat(const CommunicationModule::ConnectionContext& context,
                                         const CommunicationModule::NetworkMessage& message) {
    json chatData;
    if (!ParseJsonMessage(message.payload, chatData)) {
        std::cerr << "【聊天消息】JSON解析失败" << std::endl;
        return;
    }
    
    std::string chatMsg = chatData.value("message", "");
    std::string channel = chatData.value("channel", "world");
    uint64_t playerId = context.clientId;
    
    std::cout << "【处理聊天】玩家 " << playerId << " 在频道[" << channel 
              << "]说: " << chatMsg << std::endl;
    
    // 这里可以实现聊天广播逻辑
    // 例如：广播给同区域玩家
}

void GameMessageHandler::HandleHeartbeat(const CommunicationModule::ConnectionContext& context,
                                        const CommunicationModule::NetworkMessage& message) {
    json heartbeatData;
    if (!ParseJsonMessage(message.payload, heartbeatData)) {
        // 心跳消息可能很简单，不是JSON格式
        std::cout << "【心跳】客户端 " << context.clientId << " 发送心跳" << std::endl;
        return;
    }
    
    uint64_t timestamp = heartbeatData.value("timestamp", 0);
    std::cout << "【心跳】客户端 " << context.clientId 
              << " 时间戳: " << timestamp << std::endl;
    
    // 这里可以更新客户端活动时间
    // 用于检测不活跃连接
}

void GameMessageHandler::HandleInternalEvent(const std::string& eventType,
                                            const std::any& data,
                                            const std::map<std::string, std::any>& metadata) {
    std::cout << "【内部事件】事件类型: " << eventType << std::endl;
    
    if (eventType == "player_state_update") {
        HandlePlayerStateUpdate(data, metadata);
    } else if (eventType == "zone_state_change") {
        HandleZoneStateChange(data, metadata);
    } else if (eventType == "system_announcement") {
        try {
            std::string announcement = std::any_cast<std::string>(data);
            std::cout << "【系统公告】: " << announcement << std::endl;
        } catch (const std::bad_any_cast&) {
            std::cerr << "【内部事件】系统公告数据格式错误" << std::endl;
        }
    } else {
        std::cout << "【内部事件】未知事件类型: " << eventType << std::endl;
    }
}

void GameMessageHandler::HandlePlayerStateUpdate(const std::any& data,
                                                const std::map<std::string, std::any>& metadata) {
    try {
        // 演示：处理玩家状态更新事件
        std::cout << "【玩家状态更新】收到状态更新事件" << std::endl;
        
        // 这里可以实现玩家状态同步逻辑
        // 例如：跨服务器玩家状态同步
        
    } catch (const std::exception& e) {
        std::cerr << "【玩家状态更新】处理错误: " << e.what() << std::endl;
    }
}

void GameMessageHandler::HandleZoneStateChange(const std::any& data,
                                              const std::map<std::string, std::any>& metadata) {
    try {
        // 演示：处理区域状态变化事件
        std::cout << "【区域状态变化】收到区域状态变化事件" << std::endl;
        
        // 这里可以实现区域状态同步逻辑
        // 例如：动态加载/卸载区域
        
    } catch (const std::exception& e) {
        std::cerr << "【区域状态变化】处理错误: " << e.what() << std::endl;
    }
}

bool GameMessageHandler::ParseJsonMessage(const std::vector<uint8_t>& data, json& result) {
    if (data.empty()) {
        return false;
    }
    
    try {
        std::string jsonStr(data.begin(), data.end());
        result = json::parse(jsonStr);
        return true;
    } catch (const json::parse_error& e) {
        std::cerr << "JSON解析错误: " << e.what() << std::endl;
        return false;
    }
}

} // namespace GameDemo