#!/bin/bash

# Intel RealSense SDK 安装脚本

set -e

echo "=========================================="
echo "安装 Intel RealSense SDK"
echo "=========================================="

# 安装基础依赖
echo "📦 安装基础依赖..."
sudo apt-get update -qq
sudo apt-get install -y -qq \
    build-essential \
    cmake \
    git \
    pkg-config \
    libssl-dev \
    libusb-1.0-0-dev \
    libudev-dev \
    wget \
    gnupg

# 安装 Fast-DDS 依赖
echo "📦 安装 Fast-DDS 依赖..."
sudo apt-get install -y -qq \
    libfastcdr-dev \
    libfastrtps-dev 2>/dev/null || echo "Fast-DDS 将从源码安装"

# 添加 Intel RealSense 仓库并修复 GPG 密钥问题
echo "🔑 配置 Intel RealSense 仓库..."

# 创建 keyrings 目录（如果不存在）
sudo mkdir -p /etc/apt/keyrings

# 下载并安装 GPG 密钥到新的位置（避免 deprecated warning）
wget -qO- https://librealsense.intel.com/Debian/librealsense.pgp | \
    sudo tee /etc/apt/keyrings/librealsense.pgp > /dev/null

# 添加仓库（使用新的 signed-by 格式）
echo "deb [signed-by=/etc/apt/keyrings/librealsense.pgp] https://librealsense.intel.com/Debian/apt-repo $(lsb_release -cs) main" | \
    sudo tee /etc/apt/sources.list.d/librealsense.list > /dev/null

# 更新并安装 RealSense
echo "📦 安装 Intel RealSense SDK..."
sudo apt-get update -qq
sudo apt-get install -y \
    librealsense2-dev \
    librealsense2-utils

# DKMS 模块是可选的（用于某些旧设备）
sudo apt-get install -y librealsense2-dkms 2>/dev/null || echo "⚠️  DKMS 模块未安装（可选）"

echo ""
echo "=========================================="
echo "✅ 安装完成！"
echo "=========================================="

# 验证安装
if pkg-config --exists realsense2; then
    echo "✅ RealSense SDK: $(pkg-config --modversion realsense2)"
else
    echo "❌ RealSense SDK 验证失败"
    exit 1
fi

# 检测设备
if command -v rs-enumerate-devices &> /dev/null; then
    echo ""
    echo "检测 RealSense 设备..."
    rs-enumerate-devices 2>/dev/null || echo "未检测到设备（如已连接请检查 USB 权限）"
fi

echo ""
echo "下一步: 重新编译项目"
echo "  ./scripts/build.sh"
echo "=========================================="
