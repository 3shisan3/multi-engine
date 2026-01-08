#include "game_server.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>

std::atomic<bool> g_running{true};

/**
 * @brief 信号处理函数
 * @note 演示优雅关闭服务器
 */
void SignalHandler(int signal) {
    std::cout << "\n收到信号 " << signal << ", 正在关闭服务器..." << std::endl;
    g_running = false;
}

/**
 * @brief 显示统计信息
 * @note 演示如何监控服务器状态
 */
void DisplayStatistics(GameDemo::GameServer& server) {
    static int counter = 0;
    
    if (++counter % 10 == 0) {  // 每10秒显示一次
        auto stats = server.GetStatistics();
        
        std::cout << "\n【服务器统计】=========================" << std::endl;
        std::cout << "服务器状态: " << stats["server_state"].get<int>() << std::endl;
        std::cout << "在线玩家: " << stats["player_count"].get<size_t>() << std::endl;
        
        auto commStats = stats["communication_statistics"];
        std::cout << "登录数: " << commStats["total_logins"].get<uint64_t>() << std::endl;
        std::cout << "消息发送: " << commStats["messages_sent"].get<uint64_t>() << std::endl;
        std::cout << "通信错误: " << commStats["communication_errors"].get<uint64_t>() << std::endl;
        std::cout << "========================================" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << " 通信模块应用演示 - 分布式游戏服务器" << std::endl;
    std::cout << " 演示如何使用通信枢纽模块" << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 设置信号处理
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
    
    try {
        // 加载配置
        std::string configFile = "config/server_config.json";
        if (argc > 1) {
            configFile = argv[1];
        }
        
        std::cout << "使用配置文件: " << configFile << std::endl;
        
        // 创建游戏服务器实例
        GameDemo::GameServer server(configFile);
        
        // 初始化服务器
        std::cout << "\n【步骤1】初始化服务器..." << std::endl;
        if (!server.Initialize()) {
            std::cerr << "服务器初始化失败" << std::endl;
            return 1;
        }
        
        // 启动服务器
        std::cout << "\n【步骤2】启动服务器..." << std::endl;
        if (!server.Start()) {
            std::cerr << "服务器启动失败" << std::endl;
            return 1;
        }
        
        // 主循环
        std::cout << "\n【运行中】服务器正在运行..." << std::endl;
        std::cout << "按 Ctrl+C 停止服务器" << std::endl;
        
        while (g_running) {
            // 显示统计信息
            DisplayStatistics(server);
            
            // 模拟一些游戏逻辑
            // 在实际应用中，这里会有游戏循环逻辑
            
            // 等待1秒
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // 停止服务器
        std::cout << "\n【步骤3】停止服务器..." << std::endl;
        server.Stop();
        
        std::cout << "\n【完成】服务器已停止" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n【错误】致命错误: " << e.what() << std::endl;
        return 1;
    }
}