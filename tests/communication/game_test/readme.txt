完整测试流程
# 终端1：启动游戏服务器
cd build_demo
./game_server_demo ../config/server_config.json

# 终端2：运行Python客户端测试
python3 ../test_client.py --host 127.0.0.1 --port 9000 --clients 5 --duration 60

# 终端3：监控服务器输出
tail -f server.log  # 如果有日志文件


基本使用
# 基本测试（单个客户端）
python3 test_client.py --host 127.0.0.1 --port 9000 --duration 30

# 多个客户端测试
python3 test_client.py --host 127.0.0.1 --port 9000 --clients 10 --duration 60

# 详细日志
python3 test_client.py --host 127.0.0.1 --port 9000 --log-level DEBUG

# 请求统计信息
python3 test_client.py --host 127.0.0.1 --port 9000 --statistics