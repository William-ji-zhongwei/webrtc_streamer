#!/bin/bash

# WebRTC Streamer - 运行脚本
# 用途：运行编译好的程序

set -e  # 遇到错误立即退出

# 获取脚本所在目录的父目录（项目根目录）
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"
BUILD_DIR="$PROJECT_ROOT/build"
EXECUTABLE="$BUILD_DIR/webrtc_streamer"

echo "=========================================="
echo "WebRTC Streamer - 启动程序"
echo "=========================================="

# 检查可执行文件是否存在
if [ ! -f "$EXECUTABLE" ]; then
    echo "错误: 未找到可执行文件"
    echo "路径: $EXECUTABLE"
    echo ""
    echo "请先编译项目:"
    echo "  ./scripts/build.sh"
    exit 1
fi

echo "可执行文件: $EXECUTABLE"
echo ""

# 预设配置
show_presets() {
    echo "可用的预设配置:"
    echo "-------------------"
    echo "1. realsense       - Intel RealSense D455 (640x480@30fps)"
    echo "2. realsense_hd    - Intel RealSense D455 (1280x720@30fps)"
    echo "3. realsense_depth - Intel RealSense D455 + 深度流"
    echo "4. camera          - USB 摄像头 (默认设备 0)"
    echo "5. custom          - 自定义参数"
    echo ""
}

# 如果没有参数，显示选择菜单
if [ $# -eq 0 ]; then
    show_presets
    read -p "请选择预设 (1-5) 或按 Enter 使用默认配置: " choice
    
    case $choice in
        1)
            ARGS="--source realsense --width 640 --height 480 --fps 30"
            ;;
        2)
            ARGS="--source realsense --width 1280 --height 720 --fps 30"
            ;;
        3)
            ARGS="--source realsense --width 640 --height 480 --fps 30 --depth"
            ;;
        4)
            ARGS="--source camera --device 0"
            ;;
        5)
            echo ""
            read -p "视频源类型 (realsense/camera/file/rtsp): " src_type
            
            if [ "$src_type" == "realsense" ] || [ "$src_type" == "camera" ]; then
                read -p "宽度 (默认: 640): " width
                read -p "高度 (默认: 480): " height
                read -p "帧率 (默认: 30): " fps
                width=${width:-640}
                height=${height:-480}
                fps=${fps:-30}
                
                ARGS="--source $src_type --width $width --height $height --fps $fps"
                
                if [ "$src_type" == "realsense" ]; then
                    read -p "启用深度流? (y/n): " enable_depth
                    if [ "$enable_depth" == "y" ]; then
                        ARGS="$ARGS --depth"
                    fi
                elif [ "$src_type" == "camera" ]; then
                    read -p "设备 ID (默认: 0): " device_id
                    device_id=${device_id:-0}
                    ARGS="$ARGS --device $device_id"
                fi
            else
                read -p "文件路径或 RTSP URL: " file_path
                ARGS="--source $src_type --file $file_path"
            fi
            
            read -p "服务器 IP (默认: 192.168.1.34): " server_ip
            read -p "服务器端口 (默认: 50061): " server_port
            server_ip=${server_ip:-192.168.1.34}
            server_port=${server_port:-50061}
            ARGS="$ARGS --server $server_ip --port $server_port"
            ;;
        *)
            # 默认配置 - RealSense
            ARGS="--source realsense"
            ;;
    esac
    
    echo ""
    echo "运行参数: $ARGS"
    echo ""
    read -p "按 Enter 继续或 Ctrl+C 取消..."
    
else
    # 使用传入的参数
    ARGS="$@"
fi

# 检查相机权限
check_permissions() {
    if ! groups | grep -q video; then
        echo "⚠️  警告: 当前用户不在 video 组中"
        echo "如果遇到权限问题，请运行:"
        echo "  sudo usermod -a -G video $USER"
        echo "然后重新登录"
        echo ""
    fi
}

check_permissions

# 如果使用 RealSense，检查设备
if [[ "$ARGS" == *"realsense"* ]]; then
    echo "检查 RealSense 设备..."
    if command -v rs-enumerate-devices &> /dev/null; then
        if rs-enumerate-devices 2>/dev/null | grep -q "Device info"; then
            echo "✓ 检测到 RealSense 设备"
        else
            echo "⚠️  警告: 未检测到 RealSense 设备"
            echo "请确保相机已连接并有权限访问"
        fi
    else
        echo "⚠️  未安装 rs-enumerate-devices 工具"
    fi
    echo ""
fi

# 运行程序
echo "启动 WebRTC Streamer..."
echo "按 Ctrl+C 停止"
echo ""
echo "=========================================="

cd "$PROJECT_ROOT"
exec "$EXECUTABLE" $ARGS
