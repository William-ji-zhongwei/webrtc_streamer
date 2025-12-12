#!/bin/bash

# WebRTC Streamer - 编译脚本
# 用途：编译项目

set -e  # 遇到错误立即退出

echo "=========================================="
echo "WebRTC Streamer - 项目编译"
echo "=========================================="

# 获取脚本所在目录的父目录（项目根目录）
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"

echo "项目根目录: $PROJECT_ROOT"
cd "$PROJECT_ROOT"

# 构建目录
BUILD_DIR="$PROJECT_ROOT/build"
BUILD_TYPE="${1:-Release}"  # 默认 Release 模式，可传参指定 Debug

echo "构建类型: $BUILD_TYPE"
echo ""

# 清理旧的构建（可选）
if [ "$2" == "clean" ]; then
    echo "清理旧的构建目录..."
    rm -rf "$BUILD_DIR"
fi

# 创建构建目录
if [ ! -d "$BUILD_DIR" ]; then
    echo "创建构建目录: $BUILD_DIR"
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

# 检查依赖
echo "检查依赖..."
echo "-------------------"

check_dependency() {
    local name=$1
    local pkg=$2
    
    if pkg-config --exists "$pkg" 2>/dev/null; then
        local version=$(pkg-config --modversion "$pkg")
        echo "✓ $name: $version"
        return 0
    else
        echo "✗ $name: 未找到"
        return 1
    fi
}

DEPS_OK=true

# OpenCV (尝试 opencv4 或 opencv)
if pkg-config --exists opencv4 2>/dev/null; then
    check_dependency "OpenCV" "opencv4" || DEPS_OK=false
elif pkg-config --exists opencv 2>/dev/null; then
    check_dependency "OpenCV" "opencv" || DEPS_OK=false
else
    echo "✗ OpenCV: 未找到"
    DEPS_OK=false
fi

# WebRTC (检查预编译库)
WEBRTC_ROOT="${WEBRTC_ROOT_DIR:-/opt/webrtc}"
if [ -d "$WEBRTC_ROOT" ]; then
    if [ -d "$WEBRTC_ROOT/include" ] || [ -d "$WEBRTC_ROOT/src" ]; then
        echo "✓ WebRTC: $WEBRTC_ROOT"
    else
        echo "⚠ WebRTC: 目录存在但结构不完整"
        echo "  请运行: sudo ./scripts/install_webrtc.sh"
    fi
else
    echo "✗ WebRTC: 未找到 ($WEBRTC_ROOT)"
    echo "  请运行: sudo ./scripts/install_webrtc.sh"
    DEPS_OK=false
fi

# RealSense (可选，如果不使用可以跳过)
if pkg-config --exists realsense2 2>/dev/null; then
    check_dependency "RealSense" "realsense2"
else
    echo "⚠ RealSense: 未找到 (可选，如不使用 RealSense 相机可忽略)"
fi

echo ""

if [ "$DEPS_OK" = false ]; then
    echo "错误: 缺少必要的依赖"
    echo "请先运行: ./scripts/setup_all.sh"
    exit 1
fi

# CMake 配置
echo "运行 CMake 配置..."
echo "-------------------"
cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      ..

echo ""
echo "开始编译..."
echo "-------------------"

# 获取 CPU 核心数
if command -v nproc &> /dev/null; then
    CORES=$(nproc)
elif command -v sysctl &> /dev/null; then
    CORES=$(sysctl -n hw.ncpu)
else
    CORES=4
fi

echo "使用 $CORES 个并行任务编译..."

# 编译
make -j"$CORES"

echo ""
echo "=========================================="
echo "编译完成！"
echo "=========================================="
echo ""
echo "可执行文件位置: $BUILD_DIR/webrtc_streamer"
echo ""
echo "下一步："
echo "1. 查看使用帮助: ./build/webrtc_streamer --help"
echo "2. 运行程序: ./scripts/run.sh"
echo "3. 或直接运行: ./build/webrtc_streamer [选项]"
echo ""
