#!/bin/bash

# WebRTC 快速测试脚本
# 用途：快速启动发送端和接收端进行测试

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"

echo "=========================================="
echo "WebRTC 快速测试工具"
echo "=========================================="
echo ""
echo "请选择要启动的端："
echo "1. 发送端 (C++ Sender)"
echo "2. 接收端 (Python Receiver)"
echo "3. 查看配置"
echo "4. 测试网络连通性"
echo ""
read -p "请选择 (1-4): " choice

case $choice in
    1)
        echo ""
        echo "=========================================="
        echo "启动发送端"
        echo "=========================================="
        
        if [ ! -f "$PROJECT_ROOT/build/webrtc_streamer" ]; then
            echo "错误: 未找到可执行文件"
            echo "请先运行: ./scripts/build.sh"
            exit 1
        fi
        
        echo ""
        echo "当前配置："
        cat "$PROJECT_ROOT/config.json" | grep -A 3 "server"
        echo ""
        
        read -p "使用此配置启动? (y/n): " confirm
        if [ "$confirm" != "y" ]; then
            echo "已取消"
            exit 0
        fi
        
        echo ""
        echo "启动发送端..."
        echo "按 Ctrl+C 停止"
        echo ""
        
        cd "$PROJECT_ROOT"
        ./build/webrtc_streamer
        ;;
        
    2)
        echo ""
        echo "=========================================="
        echo "启动接收端"
        echo "=========================================="
        
        if ! python3 -c "import aiortc" 2>/dev/null; then
            echo "错误: 缺少依赖库"
            echo ""
            read -p "是否现在安装? (y/n): " install
            if [ "$install" = "y" ]; then
                echo "安装依赖..."
                pip3 install aiortc opencv-python numpy av
            else
                echo "请手动安装: pip3 install aiortc opencv-python numpy av"
                exit 1
            fi
        fi
        
        echo ""
        echo "接收端将监听在:"
        echo "  IP: 0.0.0.0 (所有网络接口)"
        echo "  端口: 50061"
        echo "  STUN/TURN: 106.14.31.123:3478"
        echo ""
        
        read -p "按 Enter 继续..."
        
        echo ""
        echo "启动接收端..."
        echo "等待发送端连接..."
        echo "按 'q' 键或 Ctrl+C 停止"
        echo ""
        
        cd "$PROJECT_ROOT"
        python3 receiver_demo.py
        ;;
        
    3)
        echo ""
        echo "=========================================="
        echo "当前配置"
        echo "=========================================="
        echo ""
        cat "$PROJECT_ROOT/config.json"
        echo ""
        echo "配置文件位置: $PROJECT_ROOT/config.json"
        echo "使用编辑器修改后重新运行即可"
        ;;
        
    4)
        echo ""
        echo "=========================================="
        echo "网络连通性测试"
        echo "=========================================="
        
        SERVER_IP=$(cat "$PROJECT_ROOT/config.json" | grep '"ip"' | head -1 | cut -d'"' -f4)
        SERVER_PORT=$(cat "$PROJECT_ROOT/config.json" | grep '"port"' | head -1 | cut -d':' -f2 | tr -d ' ,')
        
        echo ""
        echo "测试目标: $SERVER_IP:$SERVER_PORT"
        echo ""
        
        echo "1. Ping 测试..."
        ping -c 3 "$SERVER_IP"
        
        echo ""
        echo "2. 端口连通性测试..."
        if command -v nc &> /dev/null; then
            timeout 3 nc -zv "$SERVER_IP" "$SERVER_PORT" 2>&1
        elif command -v telnet &> /dev/null; then
            timeout 3 telnet "$SERVER_IP" "$SERVER_PORT" 2>&1
        else
            echo "未找到 nc 或 telnet 工具"
        fi
        
        echo ""
        echo "3. STUN 服务器测试..."
        if command -v stunclient &> /dev/null; then
            stunclient 106.14.31.123 3478
        else
            echo "未安装 stunclient，跳过"
            echo "安装方法: sudo apt-get install stuntman-client"
        fi
        
        echo ""
        echo "测试完成"
        ;;
        
    *)
        echo "无效选择"
        exit 1
        ;;
esac
