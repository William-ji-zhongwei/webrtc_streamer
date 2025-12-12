#!/bin/bash

# WebRTC 预编译库安装脚本
# 下载并安装社区预编译的 WebRTC 库

set -e

echo "=========================================="
echo "安装 WebRTC 预编译库"
echo "=========================================="

# 配置变量
WEBRTC_VERSION="120.6099.4.0"  # 使用 crow-misia/libwebrtc-bin 的版本号
INSTALL_DIR="/opt/webrtc"
TEMP_DIR="/tmp/webrtc-install"

# 检测系统架构
ARCH=$(uname -m)
if [ "$ARCH" = "x86_64" ]; then
    ARCH_NAME="x64"
elif [ "$ARCH" = "aarch64" ]; then
    ARCH_NAME="arm64"
else
    echo "❌ 不支持的架构: $ARCH"
    exit 1
fi

echo "📋 系统信息:"
echo "   架构: $ARCH ($ARCH_NAME)"
echo "   版本: $WEBRTC_VERSION"
echo "   安装目录: $INSTALL_DIR"

# 创建临时目录
echo "📁 创建临时目录..."
mkdir -p "$TEMP_DIR"
cd "$TEMP_DIR"

# 安装依赖
echo "📦 安装必要的依赖..."
sudo apt-get install -y \
    wget \
    tar \
    xz-utils \
    build-essential \
    pkg-config \
    libglib2.0-dev \
    libgtk-3-dev \
    libnss3 \
    libasound2 \
    libpulse0 \
    libxcomposite1 \
    libxdamage1 \
    libxrandr2 \
    libgbm1 \
    libpango-1.0-0 \
    libcairo2 \
    libatspi2.0-0

# 下载预编译的 WebRTC 库
echo "⬇️  下载 WebRTC 预编译库..."

# 定义下载源列表
DOWNLOAD_SOURCES=(
    "https://github.com/crow-misia/libwebrtc-bin/releases/download/${WEBRTC_VERSION}/libwebrtc-linux-${ARCH_NAME}.tar.xz"
)

# 尝试从各个源下载
DOWNLOAD_SUCCESS=false
for DOWNLOAD_URL in "${DOWNLOAD_SOURCES[@]}"; do
    echo "   尝试: $DOWNLOAD_URL"
    
    if wget --timeout=30 --tries=2 "$DOWNLOAD_URL" -O webrtc.tar.xz; then
        # 验证下载的文件是否为有效的压缩包
        if tar -tf webrtc.tar.xz >/dev/null 2>&1; then
            echo "✅ 下载成功且文件校验通过！"
            DOWNLOAD_SUCCESS=true
            break
        else
            echo "⚠️  下载的文件无效 (可能是 HTML 错误页面)，尝试下一个源..."
            rm -f webrtc.tar.xz
        fi
    else
        echo "   ✗ 下载失败，尝试下一个源..."
    fi
done

# 如果所有源都失败
if [ "$DOWNLOAD_SUCCESS" = false ]; then
    echo ""
    echo "❌ 所有下载源都失败"
    echo ""
    echo "请手动下载 WebRTC 预编译库:"
    read -p "已手动下载到 $TEMP_DIR/webrtc.tar.xz? (y/n) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "请下载后重新运行脚本"
        exit 1
    fi
    
    # 验证文件是否存在
    if [ ! -f "$TEMP_DIR/webrtc.tar.xz" ]; then
        echo "❌ 未找到文件: $TEMP_DIR/webrtc.tar.xz"
        exit 1
    fi
fi

# 解压
echo "📦 解压 WebRTC 库..."
sudo mkdir -p "$INSTALL_DIR"

# 检测是否包含顶层目录
# 获取第一行路径
FIRST_PATH=$(tar -tf webrtc.tar.xz | head -n 1)
# 获取第一级目录名
TOP_DIR=$(echo "$FIRST_PATH" | cut -d/ -f1)

if [[ "$TOP_DIR" == "include" || "$TOP_DIR" == "lib" ]]; then
    echo "   检测到直接结构 (无顶层目录)"
    STRIP_OPT=""
else
    echo "   检测到嵌套结构 (顶层目录: $TOP_DIR)"
    STRIP_OPT="--strip-components=1"
fi

sudo tar -xf webrtc.tar.xz -C "$INSTALL_DIR" $STRIP_OPT

# 检查目录结构
echo "📋 检查安装结构..."
if [ -d "$INSTALL_DIR/include" ] && [ -d "$INSTALL_DIR/lib" ]; then
    echo "✅ 标准结构已识别 (include/ 和 lib/)"
elif [ -d "$INSTALL_DIR/src" ]; then
    echo "✅ 源代码结构已识别 (src/)"
else
    echo "⚠️  结构不标准，请检查: $INSTALL_DIR"
    ls -la "$INSTALL_DIR"
fi

# 设置权限
echo "🔐 设置权限..."
sudo chmod -R 755 "$INSTALL_DIR"

# 创建 pkg-config 文件（如果需要）
echo "📝 创建 pkg-config 配置..."
sudo mkdir -p /usr/local/lib/pkgconfig

sudo tee /usr/local/lib/pkgconfig/webrtc.pc > /dev/null <<EOF
prefix=$INSTALL_DIR
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: WebRTC
Description: WebRTC Native API
Version: $WEBRTC_VERSION
Libs: -L\${libdir} -lwebrtc -pthread -ldl -lX11 -lXcomposite -lXext -lXrender
Cflags: -I\${includedir} -I\${includedir}/third_party/abseil-cpp
EOF

# 更新 ld 缓存
echo "🔄 更新链接器缓存..."
echo "$INSTALL_DIR/lib" | sudo tee /etc/ld.so.conf.d/webrtc.conf > /dev/null
sudo ldconfig

# 清理y
echo "🧹 清理临时文件..."
cd /
rm -rf "$TEMP_DIR"

echo ""
echo "=========================================="
echo "✅ WebRTC 安装完成!"
echo "=========================================="
echo "安装路径: $INSTALL_DIR"
echo ""
echo "环境变量设置建议:"
echo "export WEBRTC_ROOT_DIR=$INSTALL_DIR"
echo ""
echo "可以将以上命令添加到 ~/.bashrc 或 ~/.zshrc"
echo ""
echo "下一步: 运行 ./scripts/build.sh 编译项目"
echo "=========================================="
