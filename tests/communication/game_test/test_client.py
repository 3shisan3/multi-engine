#!/usr/bin/env python3
"""
通信模块测试客户端 - Python版本
用于测试游戏服务器的通信功能
"""

import socket
import json
import time
import threading
import random
import sys
import struct
import argparse
from typing import Dict, Any, List, Optional, Tuple, Callable
from dataclasses import dataclass
from datetime import datetime
from enum import Enum

class MessageType(Enum):
    """消息类型枚举"""
    CONTROL = 0
    DATA = 1
    COMMAND = 2
    STATUS = 3
    EVENT = 4
    HEARTBEAT = 5
    CUSTOM = 6

@dataclass
class ClientConfig:
    """客户端配置"""
    server_host: str = "127.0.0.1"
    server_port: int = 9000
    client_id: int = 1
    client_name: str = "TestClient"
    reconnect_interval: int = 3
    max_reconnect_attempts: int = 5
    heartbeat_interval: int = 5  # 秒
    simulation_duration: int = 60  # 秒
    log_level: str = "INFO"

class GameClient:
    """游戏客户端模拟器"""
    
    def __init__(self, config: ClientConfig):
        self.config = config
        self.socket = None
        self.connected = False
        self.running = False
        self.sequence_id = 0
        self.reconnect_attempts = 0
        self.message_count = 0
        self.start_time = None
        self.stats = {
            "messages_sent": 0,
            "messages_received": 0,
            "errors": 0,
            "connection_time": 0
        }
        
        # 消息处理回调
        self.message_handlers = {
            "player_login": self._handle_login_response,
            "player_move": self._handle_move_response,
            "player_chat": self._handle_chat_response,
            "heartbeat": self._handle_heartbeat_response,
            "system_message": self._handle_system_message
        }
        
        # 客户端状态
        self.position = {"x": 100.0, "y": 0.0, "z": 100.0}
        self.zone = "main_city"
        self.last_activity = time.time()
        
        self._setup_logging()
    
    def _setup_logging(self):
        """设置日志"""
        log_levels = {
            "DEBUG": 10,
            "INFO": 20,
            "WARNING": 30,
            "ERROR": 40,
            "CRITICAL": 50
        }
        self.log_level = log_levels.get(self.config.log_level.upper(), 20)
    
    def log(self, level: str, message: str):
        """记录日志"""
        log_levels = {
            "DEBUG": 10,
            "INFO": 20,
            "WARNING": 30,
            "ERROR": 40,
            "CRITICAL": 50
        }
        
        if log_levels.get(level.upper(), 0) >= self.log_level:
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            print(f"[{timestamp}] [{level}] Client{self.config.client_id}: {message}")
    
    def connect(self) -> bool:
        """连接到服务器"""
        self.log("INFO", f"尝试连接到服务器 {self.config.server_host}:{self.config.server_port}")
        
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(10.0)
            self.socket.connect((self.config.server_host, self.config.server_port))
            
            self.connected = True
            self.reconnect_attempts = 0
            self.start_time = time.time()
            
            self.log("INFO", "连接成功")
            return True
            
        except Exception as e:
            self.log("ERROR", f"连接失败: {e}")
            self.stats["errors"] += 1
            return False
    
    def disconnect(self):
        """断开连接"""
        if self.socket:
            try:
                self.socket.close()
                self.log("INFO", "连接已关闭")
            except:
                pass
        
        self.connected = False
        self.socket = None
    
    def reconnect(self) -> bool:
        """重新连接"""
        if self.reconnect_attempts >= self.config.max_reconnect_attempts:
            self.log("ERROR", f"达到最大重连次数 ({self.config.max_reconnect_attempts})")
            return False
        
        self.reconnect_attempts += 1
        self.log("INFO", f"尝试重连 ({self.reconnect_attempts}/{self.config.max_reconnect_attempts})")
        
        self.disconnect()
        time.sleep(self.config.reconnect_interval)
        
        return self.connect()
    
    def send_message(self, topic: str, data: Dict[str, Any]) -> bool:
        """发送消息到服务器"""
        if not self.connected or not self.socket:
            self.log("WARNING", "未连接，无法发送消息")
            return False
        
        try:
            # 构建消息
            message = {
                "topic": topic,
                "data": data,
                "client_id": self.config.client_id,
                "timestamp": int(time.time() * 1000),
                "sequence_id": self.sequence_id
            }
            
            # 序列化为JSON
            json_str = json.dumps(message, ensure_ascii=False)
            
            # 构建协议包: [消息长度(4字节)][JSON数据]
            message_bytes = json_str.encode('utf-8')
            message_len = len(message_bytes)
            
            # 使用网络字节序打包长度
            header = struct.pack('!I', message_len)
            
            # 发送数据
            self.socket.sendall(header + message_bytes)
            
            self.sequence_id += 1
            self.stats["messages_sent"] += 1
            self.message_count += 1
            self.last_activity = time.time()
            
            self.log("DEBUG", f"发送消息: {topic} (大小: {message_len} 字节)")
            return True
            
        except Exception as e:
            self.log("ERROR", f"发送消息失败: {e}")
            self.stats["errors"] += 1
            self.connected = False
            return False
    
    def receive_message(self, timeout: float = 1.0) -> Optional[Dict[str, Any]]:
        """接收服务器消息"""
        if not self.connected or not self.socket:
            return None
        
        try:
            self.socket.settimeout(timeout)
            
            # 读取消息长度
            header = self.socket.recv(4)
            if not header:
                return None
            
            message_len = struct.unpack('!I', header)[0]
            
            # 读取消息内容
            chunks = []
            bytes_received = 0
            
            while bytes_received < message_len:
                chunk = self.socket.recv(min(message_len - bytes_received, 4096))
                if not chunk:
                    break
                chunks.append(chunk)
                bytes_received += len(chunk)
            
            if bytes_received < message_len:
                self.log("ERROR", f"消息不完整: 期望 {message_len}, 收到 {bytes_received}")
                return None
            
            message_bytes = b''.join(chunks)
            
            # 解析JSON
            message = json.loads(message_bytes.decode('utf-8'))
            
            self.stats["messages_received"] += 1
            self.last_activity = time.time()
            
            self.log("DEBUG", f"收到消息: {message.get('topic', 'unknown')}")
            return message
            
        except socket.timeout:
            return None
        except Exception as e:
            self.log("ERROR", f"接收消息失败: {e}")
            self.stats["errors"] += 1
            return None
    
    def login(self) -> bool:
        """玩家登录"""
        login_data = {
            "player_id": self.config.client_id,
            "account": f"test_account_{self.config.client_id}",
            "name": f"Player_{self.config.client_id}",
            "version": "1.0.0",
            "client_type": "python",
            "timestamp": int(time.time() * 1000)
        }
        
        self.log("INFO", f"尝试登录: {login_data['name']}")
        return self.send_message("player_login", login_data)
    
    def logout(self) -> bool:
        """玩家登出"""
        logout_data = {
            "player_id": self.config.client_id,
            "reason": "client_exit",
            "timestamp": int(time.time() * 1000)
        }
        
        self.log("INFO", "尝试登出")
        return self.send_message("player_logout", logout_data)
    
    def move(self, x: Optional[float] = None, y: Optional[float] = None, z: Optional[float] = None) -> bool:
        """玩家移动"""
        if x is None:
            x = self.position["x"] + random.uniform(-50, 50)
        if y is None:
            y = self.position["y"]
        if z is None:
            z = self.position["z"] + random.uniform(-50, 50)
        
        self.position = {"x": x, "y": y, "z": z}
        
        move_data = {
            "x": x,
            "y": y,
            "z": z,
            "player_id": self.config.client_id,
            "timestamp": int(time.time() * 1000),
            "zone": self.zone
        }
        
        self.log("DEBUG", f"移动到位置: ({x:.2f}, {y:.2f}, {z:.2f})")
        return self.send_message("player_move", move_data)
    
    def chat(self, message: Optional[str] = None, channel: str = "world") -> bool:
        """玩家聊天"""
        if message is None:
            messages = [
                f"大家好！我是Player_{self.config.client_id}",
                "今天天气真好",
                "有人一起组队吗？",
                "这个任务怎么做？",
                "需要帮忙的喊我",
                "准备去打副本了",
                "有交易的吗？",
                "公会收人啦！"
            ]
            message = random.choice(messages)
        
        chat_data = {
            "message": message,
            "player_id": self.config.client_id,
            "channel": channel,
            "timestamp": int(time.time() * 1000)
        }
        
        self.log("INFO", f"发送聊天: {message}")
        return self.send_message("player_chat", chat_data)
    
    def send_heartbeat(self) -> bool:
        """发送心跳"""
        heartbeat_data = {
            "player_id": self.config.client_id,
            "timestamp": int(time.time() * 1000),
            "status": "alive"
        }
        
        self.log("DEBUG", "发送心跳")
        return self.send_message("heartbeat", heartbeat_data)
    
    def request_statistics(self) -> bool:
        """请求服务器统计信息"""
        stats_data = {
            "player_id": self.config.client_id,
            "request_type": "server_stats",
            "timestamp": int(time.time() * 1000)
        }
        
        self.log("INFO", "请求服务器统计信息")
        return self.send_message("statistics_request", stats_data)
    
    def _handle_login_response(self, message: Dict[str, Any]):
        """处理登录响应"""
        data = message.get("data", {})
        
        if data.get("success", False):
            player_info = data.get("player_info", {})
            self.log("INFO", f"登录成功! 欢迎 {player_info.get('name', '玩家')}")
            self.log("INFO", f"位置: {player_info.get('position', '未知')}")
            self.log("INFO", f"区域: {player_info.get('zone', '未知')}")
        else:
            error_msg = data.get("error", "未知错误")
            self.log("ERROR", f"登录失败: {error_msg}")
    
    def _handle_move_response(self, message: Dict[str, Any]):
        """处理移动响应"""
        data = message.get("data", {})
        
        if data.get("success", False):
            self.log("DEBUG", "移动成功")
        else:
            error_msg = data.get("error", "未知错误")
            self.log("WARNING", f"移动失败: {error_msg}")
    
    def _handle_chat_response(self, message: Dict[str, Any]):
        """处理聊天响应"""
        data = message.get("data", {})
        sender = data.get("sender", "未知")
        content = data.get("content", "")
        
        if sender != f"Player_{self.config.client_id}":
            self.log("INFO", f"[{sender}]: {content}")
    
    def _handle_heartbeat_response(self, message: Dict[str, Any]):
        """处理心跳响应"""
        data = message.get("data", {})
        
        if data.get("status") == "ok":
            self.log("DEBUG", "心跳响应正常")
        else:
            self.log("WARNING", "心跳响应异常")
    
    def _handle_system_message(self, message: Dict[str, Any]):
        """处理系统消息"""
        data = message.get("data", {})
        msg_type = data.get("type", "info")
        content = data.get("content", "")
        
        if msg_type == "info":
            self.log("INFO", f"系统消息: {content}")
        elif msg_type == "warning":
            self.log("WARNING", f"系统警告: {content}")
        elif msg_type == "error":
            self.log("ERROR", f"系统错误: {content}")
        elif msg_type == "announcement":
            self.log("INFO", f"【公告】{content}")
        else:
            self.log("INFO", f"系统消息({msg_type}): {content}")
    
    def process_incoming_messages(self):
        """处理传入消息"""
        try:
            while self.running and self.connected:
                message = self.receive_message(timeout=0.1)
                if not message:
                    continue
                
                topic = message.get("topic")
                if topic in self.message_handlers:
                    self.message_handlers[topic](message)
                else:
                    self.log("DEBUG", f"收到未知主题的消息: {topic}")
                    
        except Exception as e:
            self.log("ERROR", f"消息处理错误: {e}")
    
    def simulate_game_behavior(self):
        """模拟游戏行为"""
        self.log("INFO", "开始模拟游戏行为")
        
        # 定义一个空操作的lambda函数
        def do_nothing():
            pass
        
        # 行为权重 - 使用元组列表
        actions = [
            (self.move, 40),           # 40% 移动
            (self.chat, 25),           # 25% 聊天
            (do_nothing, 20),          # 20% 空闲
            (self.send_heartbeat, 15)  # 15% 心跳
        ]
        
        start_time = time.time()
        last_heartbeat = start_time
        
        while self.running and (time.time() - start_time) < self.config.simulation_duration:
            try:
                # 定期发送心跳
                current_time = time.time()
                if current_time - last_heartbeat >= self.config.heartbeat_interval:
                    self.send_heartbeat()
                    last_heartbeat = current_time
                
                # 选择行为
                total_weight = sum(weight for _, weight in actions)
                choice = random.uniform(0, total_weight)
                
                cumulative_weight = 0
                selected_action = do_nothing  # 默认空操作
                
                for action, weight in actions:
                    cumulative_weight += weight
                    if choice <= cumulative_weight:
                        selected_action = action
                        break
                
                # 执行选中的行为
                selected_action()
                
                # 随机延迟
                delay = random.uniform(0.5, 2.0)
                time.sleep(delay)
                
                # 显示进度
                elapsed = time.time() - start_time
                if int(elapsed) % 10 == 0 and int(elapsed) > 0:
                    self.log("INFO", f"模拟进行中... {int(elapsed)}/{self.config.simulation_duration}秒")
                    
            except Exception as e:
                self.log("ERROR", f"行为模拟错误: {e}")
                break
    
    def run(self):
        """运行客户端"""
        self.running = True
        self.log("INFO", "启动客户端模拟器")
        
        try:
            # 连接服务器
            if not self.connect():
                if not self.reconnect():
                    self.log("ERROR", "无法连接到服务器")
                    return False
            
            # 启动消息接收线程
            receive_thread = threading.Thread(
                target=self.process_incoming_messages,
                daemon=True
            )
            receive_thread.start()
            
            # 登录
            if not self.login():
                self.log("ERROR", "登录失败")
                return False
            
            # 等待登录响应
            time.sleep(1)
            
            # 模拟游戏行为
            self.simulate_game_behavior()
            
            # 登出
            self.logout()
            time.sleep(0.5)
            
            # 显示统计信息
            self.show_statistics()
            
            return True
            
        except KeyboardInterrupt:
            self.log("INFO", "用户中断")
            return True
        except Exception as e:
            self.log("ERROR", f"客户端运行错误: {e}")
            return False
        finally:
            self.running = False
            self.disconnect()
    
    def show_statistics(self):
        """显示统计信息"""
        if self.start_time:
            self.stats["connection_time"] = time.time() - self.start_time
        
        self.log("INFO", "=" * 50)
        self.log("INFO", "客户端统计信息")
        self.log("INFO", "=" * 50)
        self.log("INFO", f"客户端ID: {self.config.client_id}")
        self.log("INFO", f"连接时间: {self.stats['connection_time']:.2f}秒")
        self.log("INFO", f"发送消息数: {self.stats['messages_sent']}")
        self.log("INFO", f"接收消息数: {self.stats['messages_received']}")
        self.log("INFO", f"错误数: {self.stats['errors']}")
        
        if self.message_count > 0:
            success_rate = self.stats['messages_sent'] / self.message_count * 100
            self.log("INFO", f"消息成功率: {success_rate:.1f}%")
        
        self.log("INFO", "=" * 50)

def run_single_client(config: ClientConfig) -> Tuple[bool, Dict[str, Any]]:
    """运行单个客户端"""
    client = GameClient(config)
    success = client.run()
    return success, client.stats

def run_multiple_clients(base_config: ClientConfig, num_clients: int) -> Dict[str, Any]:
    """运行多个客户端"""
    import concurrent.futures
    
    print(f"启动 {num_clients} 个客户端模拟器")
    
    clients = []
    results = []
    
    with concurrent.futures.ThreadPoolExecutor(max_workers=min(num_clients, 10)) as executor:
        futures = []
        
        for i in range(1, num_clients + 1):
            client_config = ClientConfig(
                server_host=base_config.server_host,
                server_port=base_config.server_port,
                client_id=i,
                client_name=f"TestClient_{i}",
                reconnect_interval=base_config.reconnect_interval,
                max_reconnect_attempts=base_config.max_reconnect_attempts,
                heartbeat_interval=base_config.heartbeat_interval,
                simulation_duration=base_config.simulation_duration,
                log_level="ERROR" if i > 1 else base_config.log_level  # 只显示第一个客户端的详细日志
            )
            
            future = executor.submit(run_single_client, client_config)
            futures.append(future)
            
            # 错开启动时间
            time.sleep(0.1)
        
        # 收集结果
        for future in concurrent.futures.as_completed(futures):
            try:
                success, stats = future.result()
                results.append((success, stats))
            except Exception as e:
                print(f"客户端错误: {e}")
                results.append((False, {}))
    
    # 汇总统计
    total_stats = {
        "total_clients": num_clients,
        "successful_clients": sum(1 for success, _ in results if success),
        "total_messages_sent": sum(stats.get("messages_sent", 0) for _, stats in results),
        "total_messages_received": sum(stats.get("messages_received", 0) for _, stats in results),
        "total_errors": sum(stats.get("errors", 0) for _, stats in results),
        "total_connection_time": sum(stats.get("connection_time", 0) for _, stats in results)
    }
    
    print("\n" + "=" * 60)
    print("多客户端测试汇总")
    print("=" * 60)
    print(f"总客户端数: {total_stats['total_clients']}")
    print(f"成功客户端数: {total_stats['successful_clients']}")
    
    if total_stats['total_clients'] > 0:
        success_rate = total_stats['successful_clients'] / total_stats['total_clients'] * 100
        print(f"成功率: {success_rate:.1f}%")
    
    print(f"总发送消息数: {total_stats['total_messages_sent']}")
    print(f"总接收消息数: {total_stats['total_messages_received']}")
    print(f"总错误数: {total_stats['total_errors']}")
    print(f"总连接时间: {total_stats['total_connection_time']:.2f}秒")
    
    if total_stats['total_connection_time'] > 0:
        msg_per_second = total_stats['total_messages_sent'] / total_stats['total_connection_time']
        print(f"消息发送速率: {msg_per_second:.1f} 条/秒")
    
    print("=" * 60)
    
    return total_stats

def generate_test_report(stats: Dict[str, Any], config: ClientConfig, output_file: str = "test_report.json"):
    """生成测试报告"""
    from datetime import datetime
    
    report = {
        "test_timestamp": datetime.now().isoformat(),
        "test_config": {
            "server_host": config.server_host,
            "server_port": config.server_port,
            "num_clients": getattr(config, 'num_clients', 1),
            "duration": config.simulation_duration,
            "heartbeat_interval": config.heartbeat_interval
        },
        "test_results": stats,
        "performance_metrics": {
            "messages_per_second": stats.get("total_messages_sent", 0) / max(stats.get("total_connection_time", 1), 1),
            "success_rate": stats.get("successful_clients", 0) / max(stats.get("total_clients", 1), 1),
            "error_rate": stats.get("total_errors", 0) / max(stats.get("total_messages_sent", 1), 1)
        }
    }
    
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(report, f, indent=2, ensure_ascii=False)
    
    print(f"测试报告已保存到: {output_file}")

def main():
    """主函数"""
    parser = argparse.ArgumentParser(description="游戏客户端模拟器")
    parser.add_argument("--host", default="127.0.0.1", help="服务器地址")
    parser.add_argument("--port", type=int, default=9000, help="服务器端口")
    parser.add_argument("--clients", type=int, default=1, help="客户端数量")
    parser.add_argument("--client-id", type=int, default=1, help="客户端ID")
    parser.add_argument("--duration", type=int, default=30, help="模拟时长(秒)")
    parser.add_argument("--log-level", default="INFO", choices=["DEBUG", "INFO", "WARNING", "ERROR"], help="日志级别")
    parser.add_argument("--heartbeat", type=int, default=5, help="心跳间隔(秒)")
    parser.add_argument("--reconnect", action="store_true", help="启用自动重连")
    parser.add_argument("--statistics", action="store_true", help="请求统计信息")
    parser.add_argument("--report", action="store_true", help="生成测试报告")
    
    args = parser.parse_args()
    
    print("=" * 60)
    print("通信模块测试客户端 - Python版本")
    print("=" * 60)
    print(f"服务器: {args.host}:{args.port}")
    print(f"客户端数: {args.clients}")
    print(f"模拟时长: {args.duration}秒")
    print(f"日志级别: {args.log_level}")
    print("=" * 60)
    
    base_config = ClientConfig(
        server_host=args.host,
        server_port=args.port,
        client_id=args.client_id,
        client_name=f"TestClient_{args.client_id}",
        reconnect_interval=3,
        max_reconnect_attempts=3 if args.reconnect else 0,
        heartbeat_interval=args.heartbeat,
        simulation_duration=args.duration,
        log_level=args.log_level
    )
    
    try:
        if args.clients > 1:
            stats = run_multiple_clients(base_config, args.clients)
            if args.report:
                base_config.num_clients = args.clients  # 临时添加属性
                generate_test_report(stats, base_config)
        else:
            client = GameClient(base_config)
            
            if args.statistics:
                # 只请求统计信息
                if client.connect():
                    client.request_statistics()
                    time.sleep(1)  # 等待响应
                    client.disconnect()
            else:
                success = client.run()
                if args.report and success:
                    generate_test_report(client.stats, base_config)
                    
    except KeyboardInterrupt:
        print("\n用户中断测试")
    except Exception as e:
        print(f"测试错误: {e}")
        import traceback
        traceback.print_exc()
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())