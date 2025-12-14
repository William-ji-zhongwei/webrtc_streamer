#!/bin/bash

# 信令服务器启动脚本

echo "🚀 Starting WebRTC Signaling Server..."

# 检查 Python 版本
if ! command -v python3 &> /dev/null; then
    echo "❌ Python3 not found. Please install Python 3.7+"
    exit 1
fi

# 检查依赖
if ! python3 -c "import websockets" 2>/dev/null; then
    echo "📦 Installing dependencies..."
    pip3 install -r requirements.txt
fi

# 启动服务器
python3 server.py
