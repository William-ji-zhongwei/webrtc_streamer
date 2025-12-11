#!/bin/bash

# WebRTC Streamer - 依赖安装脚本
# 用途：安装项目所需的所有依赖

set -e  # 遇到错误立即退出

echo "=========================================="
echo "WebRTC Streamer - 依赖安装"
echo "=========================================="

# 检测系统类型
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$NAME
    VER=$VERSION_ID
else
    echo "无法检测操作系统类型"
    exit 1
fi

echo "检测到系统: $OS $VER"
echo ""

# Ubuntu/Debian 系统
if [[ "$OS" == *"Ubuntu"* ]] || [[ "$OS" == *"Debian"* ]]; then
    echo "正在更新软件包列表..."
    sudo apt-get update

    echo ""
    echo "安装基础编译工具..."
    sudo apt-get install -y \
        build-essential \
        cmake \
        git \
        pkg-config \
        wget

    echo ""
    echo "安装 OpenCV..."
    sudo apt-get install -y \
        libopencv-dev \
        libopencv-contrib-dev

    echo ""
    echo "安装 Intel RealSense SDK 依赖..."
    sudo apt-get install -y \
        libusb-1.0-0-dev \
        libudev-dev \
        libssl-dev
    
    # 检查是否已添加 RealSense 仓库
    if ! grep -q "librealsense" /etc/apt/sources.list /etc/apt/sources.list.d/* 2>/dev/null; then
        echo "添加 Intel RealSense 仓库..."
        sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-key F6E65AC044F831AC80A06380C8B3A55A6F3EFCDE || true
        sudo add-apt-repository "deb https://librealsense.intel.com/Debian/apt-repo $(lsb_release -cs) main" -y
        sudo apt-get update
    fi
    
    echo ""
    echo "安装 Intel RealSense SDK..."
    sudo apt-get install -y \
        librealsense2-dev \
        librealsense2-utils \
        librealsense2-dkms

    echo ""
    echo "安装 Fast-DDS 依赖 (RealSense 需要)..."
    sudo apt-get install -y \
        libfastcdr-dev \
        libfastrtps-dev \
        libboost-all-dev || {
        echo "⚠️  警告: 部分依赖安装失败"
        echo "尝试从源码安装 Fast-DDS..."
        
        # 安装 Fast-CDR
        if [ ! -d /tmp/Fast-CDR ]; then
            cd /tmp
            git clone https://github.com/eProsima/Fast-CDR.git
            cd Fast-CDR
            mkdir -p build && cd build
            cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
            make -j$(nproc)
            sudo make install
            sudo ldconfig
        fi
        
        # 安装 Fast-DDS
        if [ ! -d /tmp/Fast-DDS ]; then
            cd /tmp
            git clone https://github.com/eProsima/Fast-DDS.git
            cd Fast-DDS
            mkdir -p build && cd build
            cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
            make -j$(nproc)
            sudo make install
            sudo ldconfig
        fi
        
        cd "$PROJECT_ROOT" || cd -
    }

# CentOS/RHEL/Fedora 系统
elif [[ "$OS" == *"CentOS"* ]] || [[ "$OS" == *"Red Hat"* ]] || [[ "$OS" == *"Fedora"* ]]; then
    echo "正在安装基础编译工具..."
    sudo yum groupinstall -y "Development Tools"
    sudo yum install -y cmake git pkg-config wget

    echo ""
    echo "安装 OpenCV..."
    sudo yum install -y opencv opencv-devel

    echo ""
    echo "警告: RealSense SDK 需要手动从源码编译"
    echo "请访问: https://github.com/IntelRealSense/librealsense"

else
    echo "不支持的操作系统: $OS"
    echo "请手动安装依赖"
    exit 1
fi

echo ""
echo "=========================================="
echo "依赖安装完成！"
echo "=========================================="
echo ""
echo "检查已安装的版本："
echo "-------------------"

# 检查工具版本
echo -n "CMake: "
cmake --version | head -n1 || echo "未安装"

echo -n "GCC: "
gcc --version | head -n1 || echo "未安装"

echo -n "OpenCV: "
pkg-config --modversion opencv4 2>/dev/null || pkg-config --modversion opencv 2>/dev/null || echo "未安装"

echo -n "RealSense: "
pkg-config --modversion realsense2 2>/dev/null || echo "未安装"

echo ""
echo "提示："
echo "1. 如果要使用 RealSense 相机，请确保已连接设备"
echo "2. 运行 'rs-enumerate-devices' 检查相机是否被识别"
echo "3. 将用户添加到 video 组: sudo usermod -a -G video \$USER"
echo "4. 重新登录使组权限生效"
echo ""
echo "下一步："
echo "运行 './scripts/build.sh' 编译项目"
