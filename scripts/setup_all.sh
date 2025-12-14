#!/bin/bash

# WebRTC Streamer - 一键设置脚本
# 自动安装所有依赖并编译项目

set -e

echo "=========================================="
echo "WebRTC Streamer - 完整安装向导"
echo "=========================================="
echo ""

# 获取脚本目录
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"

cd "$PROJECT_ROOT"

# 检查是否为 root
if [ "$EUID" -eq 0 ]; then 
    echo "⚠️  请不要使用 root 用户运行此脚本"
    echo "脚本会在需要时自动请求 sudo 权限"
    exit 1
fi

# 步骤 1: 给所有脚本添加执行权限
echo "步骤 1/5: 设置脚本权限..."
echo "-------------------"
chmod +x scripts/*.sh
echo "✅ 脚本权限设置完成"
echo ""

sudo apt-get update

# 步骤 2: 安装基础依赖
echo "步骤 2/5: 安装基础依赖..."
echo "-------------------"
read -p "是否安装 WebRTC 依赖? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    sudo ./scripts/install_webrtc_deps.sh
    echo "✅ 基础依赖安装完成"
else
    echo "⏭️  跳过基础依赖安装"
fi
echo ""

# 步骤 3: 安装 WebRTC 预编译库
echo "步骤 3/5: 安装 WebRTC 预编译库..."
echo "-------------------"
if [ -d "/opt/webrtc" ]; then
    echo "检测到 WebRTC 已安装在 /opt/webrtc"
    read -p "是否重新安装? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        sudo ./scripts/install_webrtc.sh
        echo "✅ WebRTC 重新安装完成"
    else
        echo "⏭️  使用现有 WebRTC 安装"
    fi
else
    echo "未检测到 WebRTC 安装"
    read -p "是否安装 WebRTC 预编译库? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        sudo ./scripts/install_webrtc.sh
        echo "✅ WebRTC 安装完成"
    else
        echo "⚠️  未安装 WebRTC，项目可能无法编译"
        echo "稍后可以手动运行: sudo ./scripts/install_webrtc.sh"
    fi
fi
echo ""

# 步骤 4: 安装 RealSense SDK (可选)
echo "步骤 4/5: 安装 RealSense SDK (可选)..."
echo "-------------------"
if pkg-config --exists realsense2 2>/dev/null; then
    echo "✅ RealSense SDK 已安装"
else
    echo "未检测到 RealSense SDK"
    echo "如果您使用 Intel RealSense D455 相机，需要安装此 SDK"
    read -p "是否安装 RealSense SDK? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        sudo ./scripts/install_realsense.sh
        echo "✅ RealSense SDK 安装完成"
    else
        echo "⏭️  跳过 RealSense SDK 安装"
        echo "如需安装，稍后可运行: sudo ./scripts/install_realsense.sh"
    fi
fi
echo ""

# 步骤 5: 编译项目
echo "步骤 5/5: 编译项目..."
echo "-------------------"
read -p "是否现在编译项目? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    ./scripts/build.sh
    echo "✅ 项目编译完成"
else
    echo "⏭️  跳过编译"
    echo "稍后可运行: ./scripts/build.sh"
fi
echo ""

echo "=========================================="
echo "✅ 安装向导完成！"
echo "=========================================="
echo ""

# 检查编译结果
if [ -f "$PROJECT_ROOT/build/webrtc_streamer" ]; then
    echo "🎉 项目已成功编译！"
    echo ""
    echo "下一步："
    echo "1. 查看帮助: ./build/webrtc_streamer --help"
    echo "2. 编辑配置: vim config/config.json"
    echo "3. 运行测试: ./scripts/quick_test.sh"
    echo "4. 启动程序: ./scripts/run.sh"
    echo ""
    echo "📚 文档："
    echo "- 快速开始: QUICKSTART.md"
    echo "- WebRTC 配置: WEBRTC_NATIVE_GUIDE.md"
    echo "- WebSocket 指南: WEBSOCKET_GUIDE.md"
else
    echo "⚠️  项目未编译或编译失败"
    echo ""
    echo "请检查："
    echo "1. 所有依赖是否已安装"
    echo "2. WebRTC 库是否正确安装在 /opt/webrtc"
    echo "3. 查看编译错误信息"
    echo ""
    echo "故障排除："
    echo "- 查看 WebRTC 安装: ls -la /opt/webrtc"
    echo "- 重新安装 WebRTC: sudo ./scripts/install_webrtc.sh"
    echo "- 清理并重新编译: ./scripts/build.sh Release clean"
    echo "- 查看文档: cat WEBRTC_NATIVE_GUIDE.md"
fi

echo "=========================================="
