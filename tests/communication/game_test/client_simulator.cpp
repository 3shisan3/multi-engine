#include <iostream>
#include <thread>
#include <vector>
#include <random>
#include <asio.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @brief 客户端模拟器
 * @note 演示如何与游戏服务器通信
 */
class ClientSimulator {
public:
    ClientSimulator(int clientId, const std::string& serverAddr, int serverPort)
        : clientId_(clientId), serverAddr_(serverAddr), serverPort_(serverPort) {}
    
    void Run() {
        try {
            std::cout << "【客户端" << clientId_ << "】开始运行" << std::endl;
            
            asio::io_context ioContext;
            asio::ip::tcp::socket socket(ioContext);
            
            // 连接服务器
            asio::ip::tcp::resolver resolver(ioContext);
            auto endpoints = resolver.resolve(serverAddr_, std::to_string(serverPort_));
            asio::connect(socket, endpoints);
            
            std::cout << "【客户端" << clientId_ << "】已连接到服务器" << std::endl;
            
            // 登录
            Login(socket);
            
            // 模拟游戏行为
            SimulateGameBehavior(socket);
            
            // 登出
            Logout(socket);
            
            socket.close();
            std::cout << "【客户端" << clientId_ << "】断开连接" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "【客户端" << clientId_ << "】错误: " << e.what() << std::endl;
        }
    }
    
private:
    void Login(asio::ip::tcp::socket& socket) {
        json loginData;
        loginData["player_id"] = clientId_;
        loginData["account"] = "account_" + std::to_string(clientId_);
        loginData["name"] = "Player_" + std::to_string(clientId_);
        loginData["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        SendMessage(socket, "player_login", loginData);
        std::cout << "【客户端" << clientId_ << "】发送登录消息" << std::endl;
        
        // 等待登录响应
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    void SimulateGameBehavior(asio::ip::tcp::socket& socket) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> actionDist(0, 100);
        std::uniform_real_distribution<> posDist(0.0, 1000.0);
        
        for (int i = 0; i < 5; ++i) {  // 执行5个动作
            int action = actionDist(gen);
            
            if (action < 40) {  // 40%概率移动
                Move(socket);
            } else if (action < 70) {  // 30%概率聊天
                Chat(socket);
            } else if (action < 90) {  // 20%概率心跳
                Heartbeat(socket);
            } else {  // 10%概率切换区域
                // 在实际应用中可能需要切换区域
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    
    void Move(asio::ip::tcp::socket& socket) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<> posDist(0.0, 1000.0);
        
        json moveData;
        moveData["x"] = posDist(gen);
        moveData["y"] = 0.0;
        moveData["z"] = posDist(gen);
        moveData["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        SendMessage(socket, "player_move", moveData);
        std::cout << "【客户端" << clientId_ << "】发送移动消息" << std::endl;
    }
    
    void Chat(asio::ip::tcp::socket& socket) {
        json chatData;
        chatData["message"] = "Hello from Player " + std::to_string(clientId_) + "!";
        chatData["channel"] = "world";
        chatData["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        SendMessage(socket, "player_chat", chatData);
        std::cout << "【客户端" << clientId_ << "】发送聊天消息" << std::endl;
    }
    
    void Heartbeat(asio::ip::tcp::socket& socket) {
        json heartbeatData;
        heartbeatData["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        SendMessage(socket, "heartbeat", heartbeatData);
        std::cout << "【客户端" << clientId_ << "】发送心跳" << std::endl;
    }
    
    void Logout(asio::ip::tcp::socket& socket) {
        json logoutData;
        logoutData["player_id"] = clientId_;
        logoutData["reason"] = "normal";
        logoutData["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        SendMessage(socket, "player_logout", logoutData);
        std::cout << "【客户端" << clientId_ << "】发送登出消息" << std::endl;
    }
    
    void SendMessage(asio::ip::tcp::socket& socket, 
                    const std::string& topic,
                    const json& data) {
        // 构建消息: [主题长度(4字节)][主题][JSON数据]
        std::string topicStr = topic;
        std::string dataStr = data.dump();
        
        // 主题长度（网络字节序）
        uint32_t topicLen = htonl(static_cast<uint32_t>(topicStr.size()));
        
        // 构建完整消息
        std::vector<char> buffer;
        buffer.resize(sizeof(topicLen) + topicStr.size() + dataStr.size());
        
        char* ptr = buffer.data();
        
        // 写入主题长度
        std::memcpy(ptr, &topicLen, sizeof(topicLen));
        ptr += sizeof(topicLen);
        
        // 写入主题
        std::memcpy(ptr, topicStr.data(), topicStr.size());
        ptr += topicStr.size();
        
        // 写入数据
        std::memcpy(ptr, dataStr.data(), dataStr.size());
        
        // 发送消息
        asio::write(socket, asio::buffer(buffer));
    }
    
private:
    int clientId_;
    std::string serverAddr_;
    int serverPort_;
};

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << " 客户端模拟器" << std::endl;
    std::cout << " 模拟多个客户端连接游戏服务器" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::string serverAddr = "127.0.0.1";
    int serverPort = 9000;
    int numClients = 5;
    
    if (argc > 1) {
        serverAddr = argv[1];
    }
    if (argc > 2) {
        serverPort = std::stoi(argv[2]);
    }
    if (argc > 3) {
        numClients = std::stoi(argv[3]);
    }
    
    std::cout << "服务器地址: " << serverAddr << ":" << serverPort << std::endl;
    std::cout << "模拟客户端数: " << numClients << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::vector<std::thread> clientThreads;
    
    // 创建并运行客户端
    for (int i = 1; i <= numClients; ++i) {
        clientThreads.emplace_back([i, serverAddr, serverPort]() {
            ClientSimulator client(i, serverAddr, serverPort);
            client.Run();
        });
        
        // 稍微错开连接时间
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // 等待所有客户端完成
    for (auto& thread : clientThreads) {
        thread.join();
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << " 所有客户端模拟完成" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}