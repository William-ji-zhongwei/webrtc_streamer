#!/bin/bash

# WebRTC Streamer - RealSense 完整依赖安装脚本
# 用途：安装 RealSense 及其所有依赖（包括 Fast-DDS）

set -e

echo "=========================================="
echo "Intel RealSense 完整依赖安装"
echo "=========================================="
echo ""
echo "此脚本将安装："
echo "1. Fast-CDR & Fast-DDS (RealSense 依赖)"
echo "2. Intel RealSense SDK"
echo ""
read -p "按 Enter 继续或 Ctrl+C 取消..."

# 获取脚本所在目录
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"

echo ""
echo "=========================================="
echo "步骤 1/3: 安装基础依赖"
echo "=========================================="

sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libusb-1.0-0-dev \
    libudev-dev

echo ""
echo "=========================================="
echo "步骤 2/3: 安装 Fast-DDS 依赖"
echo "=========================================="

echo "安装 Fast-CDR 和 Fast-RTPS (Fast-DDS) 库..."
sudo apt-get install -y \
    libfastcdr-dev \
    libfastrtps-dev

echo ""
echo "=========================================="
echo "步骤 3/3: 安装 Intel RealSense SDK"
echo "=========================================="

# 添加 RealSense 仓库
if ! grep -q "librealsense" /etc/apt/sources.list /etc/apt/sources.list.d/* 2>/dev/null; then
    echo "添加 Intel RealSense 仓库..."
    sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-key F6E65AC044F831AC80A06380C8B3A55A6F3EFCDE || true
    sudo add-apt-repository "deb https://librealsense.intel.com/Debian/apt-repo $(lsb_release -cs) main" -y
    sudo apt-get update
fi

sudo apt-get install -y \
    librealsense2-dev \
    librealsense2-utils \
    librealsense2-dkms

echo ""
echo "=========================================="
echo "安装完成！"
echo "=========================================="
echo ""
echo "验证安装："
echo "-------------------"

echo -n "Fast-CDR: "
pkg-config --modversion fastcdr 2>/dev/null || echo "已安装 (无 pkg-config)"

echo -n "Fast-RTPS: "
pkg-config --modversion fastrtps 2>/dev/null || echo "已安装 (无 pkg-config)"

echo -n "RealSense: "
pkg-config --modversion realsense2 2>/dev/null || echo "未找到"

echo ""
echo "检查 RealSense 设备："
if command -v rs-enumerate-devices &> /dev/null; then
    rs-enumerate-devices || echo "未检测到设备"
else
    echo "rs-enumerate-devices 工具未找到"
fi

echo ""
echo "=========================================="
echo "下一步："
echo "1. 重新编译项目以启用 RealSense 支持："
echo "   cd $PROJECT_ROOT"
echo "   ./scripts/build.sh clean"
echo "   ./scripts/build.sh"
echo ""
echo "2. 或手动编译："
echo "   cd build"
echo "   cmake .. -DENABLE_REALSENSE=ON"
echo "   make -j\$(nproc)"
echo "=========================================="
