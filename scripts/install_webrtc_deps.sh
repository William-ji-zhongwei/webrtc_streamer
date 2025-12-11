#!/bin/bash

# WebRTC + H.265 编解码器安装脚本

set -e

echo "=========================================="
echo "安装 WebRTC 和 H.265 编解码器依赖"
echo "=========================================="

# 更新包列表
echo "📦 更新包列表..."
sudo apt-get update

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

# 安装 FFmpeg 开发库 (H.265 解码)
echo "📹 安装 FFmpeg 库..."
sudo apt-get install -y \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswscale-dev \
    libavfilter-dev

# 安装 x265 (H.265 编码)
echo "🎬 安装 x265 编码器..."
sudo apt-get install -y \
    libx265-dev \
    x265

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
    git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git "$DEPOT_TOOLS_DIR"
    echo "export PATH=\$PATH:$DEPOT_TOOLS_DIR" >> ~/.bashrc
    export PATH=$PATH:$DEPOT_TOOLS_DIR
else
    echo "depot_tools 已存在"
fi

# 可选：编译 WebRTC (这需要很长时间和大量磁盘空间)
echo ""
echo "=========================================="
echo "WebRTC 编译说明"
echo "=========================================="
echo ""
echo "如果需要完整的 WebRTC 原生 API，请运行以下命令："
echo ""
echo "  mkdir -p ~/webrtc"
echo "  cd ~/webrtc"
echo "  fetch --nohooks webrtc"
echo "  cd src"
echo "  gclient sync"
echo "  gn gen out/Default --args='is_debug=false'"
echo "  ninja -C out/Default"
echo ""
echo "注意：WebRTC 编译需要："
echo "  - 至少 16GB RAM"
echo "  - 至少 30GB 磁盘空间"
echo "  - 2-4 小时编译时间"
echo ""
echo "或者使用预编译的 WebRTC 库："
echo "  - 下载: https://webrtc.googlesource.com/src"
echo "  - 或使用 Docker: webrtc/build"
echo ""

# 安装 Python 依赖 (接收端)
echo "🐍 安装 Python 依赖..."
pip3 install --user \
    aiortc \
    opencv-python \
    numpy \
    websockets \
    av

echo ""
echo "=========================================="
echo "✅ 依赖安装完成！"
echo "=========================================="
echo ""
echo "下一步："
echo "1. 如果需要完整 WebRTC，按照上述说明编译"
echo "2. 或者使用简化版本 (不需要完整 WebRTC)"
echo "3. 构建项目: ./scripts/build.sh"
echo ""
