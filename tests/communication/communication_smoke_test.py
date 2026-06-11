#!/usr/bin/env python3
"""
通信模块 CTest 冒烟测试。

启动 game_test 服务端，运行现有 Python 客户端做一次最小 TCP 登录/消息流程，
并检查服务端确实解析到了客户端主题与登录数据。
"""

import argparse
import json
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def find_free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def wait_for_port(host: str, port: int, timeout: float) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection((host, port), timeout=0.25):
                return True
        except OSError:
            time.sleep(0.1)
    return False


def read_text(path: Path) -> str:
    if not path.exists():
        return ""
    return path.read_text(encoding="utf-8", errors="replace")


def main() -> int:
    parser = argparse.ArgumentParser(description="通信模块冒烟测试")
    parser.add_argument("--server", required=True, help="game_test 可执行文件路径")
    parser.add_argument("--config", required=True, help="服务端配置模板路径")
    parser.add_argument("--client", required=True, help="Python 客户端测试脚本路径")
    parser.add_argument("--duration", type=int, default=2, help="客户端模拟时长")
    args = parser.parse_args()

    server = Path(args.server)
    config_template = Path(args.config)
    client = Path(args.client)

    if not server.exists():
        print(f"server not found: {server}", file=sys.stderr)
        return 2
    if not config_template.exists():
        print(f"config not found: {config_template}", file=sys.stderr)
        return 2
    if not client.exists():
        print(f"client not found: {client}", file=sys.stderr)
        return 2

    port = find_free_port()
    config = json.loads(config_template.read_text(encoding="utf-8"))
    config["server_port"] = port

    with tempfile.TemporaryDirectory(prefix="me_comm_smoke_") as temp_dir:
        temp_path = Path(temp_dir)
        config_path = temp_path / "server_config.json"
        server_log_path = temp_path / "server.log"
        client_log_path = temp_path / "client.log"
        config_path.write_text(json.dumps(config, ensure_ascii=False, indent=2), encoding="utf-8")

        with server_log_path.open("w", encoding="utf-8") as server_log:
            server_proc = subprocess.Popen(
                [str(server), str(config_path)],
                stdout=server_log,
                stderr=subprocess.STDOUT,
                text=True,
            )

        try:
            if not wait_for_port("127.0.0.1", port, timeout=10.0):
                server_output = read_text(server_log_path)
                print("server did not open TCP port", file=sys.stderr)
                print(server_output, file=sys.stderr)
                return 3

            with client_log_path.open("w", encoding="utf-8") as client_log:
                client_proc = subprocess.run(
                    [
                        sys.executable,
                        str(client),
                        "--host",
                        "127.0.0.1",
                        "--port",
                        str(port),
                        "--duration",
                        str(args.duration),
                        "--log-level",
                        "ERROR",
                    ],
                    stdout=client_log,
                    stderr=subprocess.STDOUT,
                    text=True,
                    timeout=max(20, args.duration + 15),
                )

            if client_proc.returncode != 0:
                print(f"client failed with code {client_proc.returncode}", file=sys.stderr)
                print(read_text(client_log_path), file=sys.stderr)
                return client_proc.returncode

        finally:
            if server_proc.poll() is None:
                server_proc.terminate()
                try:
                    server_proc.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    server_proc.kill()
                    server_proc.wait(timeout=5)

        server_output = read_text(server_log_path)
        client_output = read_text(client_log_path)

        required_markers = ["主题: player_login", "【处理登录】"]
        missing = [marker for marker in required_markers if marker not in server_output]
        if missing:
            print(f"server log missing markers: {missing}", file=sys.stderr)
            print("--- server log ---", file=sys.stderr)
            print(server_output, file=sys.stderr)
            print("--- client log ---", file=sys.stderr)
            print(client_output, file=sys.stderr)
            return 4

        print(f"communication smoke test passed on TCP port {port}")
        return 0


if __name__ == "__main__":
    sys.exit(main())
