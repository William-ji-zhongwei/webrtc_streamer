#!/bin/bash

# WebRTC + H.264 编解码器安装脚本

set -e

echo "=========================================="
echo "安装 WebRTC 和 H.264 编解码器依赖"
echo "=========================================="

# 安装基础编译工具
echo "🔧 安装编译工具..."
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    ninja-build \
    python3 \
    python3-pip

# 安装 OpenCV 所需的 OpenGL 库
echo "🖼️  安装 OpenGL 库..."
sudo apt-get install -y \
    libgl1 \
    libglib2.0-0

# 安装 OpenCV
echo "📹 安装 OpenCV..."
sudo apt-get install -y \
    libopencv-dev \
    python3-opencv

# 安装 WebRTC 依赖
echo "🌐 安装 WebRTC 依赖..."
sudo apt-get install -y \
    libasound2-dev \
    libpulse-dev \
    libjpeg-dev \
    libopus-dev \
    libvpx-dev \
    libssl-dev \
    libnss3-dev

# 安装 depot_tools (用于编译 WebRTC)
echo "🛠️  安装 depot_tools..."
DEPOT_TOOLS_DIR="$HOME/depot_tools"
if [ ! -d "$DEPOT_TOOLS_DIR" ]; then
    # 尝试 GitHub 代理
    if git clone https://ghproxy.com/https://chromium.googlesource.com/chromium/tools/depot_tools.git "$DEPOT_TOOLS_DIR" 2>/dev/null; then
        echo "✅ 从 GitHub 代理克隆成功"
    # 原始源
    else
        echo "从官方源克隆..."
        git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git "$DEPOT_TOOLS_DIR"
    fi
    echo "export PATH=\$PATH:$DEPOT_TOOLS_DIR" >> ~/.bashrc
    export PATH=$PATH:$DEPOT_TOOLS_DIR
else
    echo "depot_tools 已存在"
fi

echo ""
echo "=========================================="
echo "✅ 依赖安装完成！"
echo "=========================================="
echo ""
