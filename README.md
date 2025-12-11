# WebRTC C++ Streamer

基于 WebRTC 的 C++ 视频流客户端，支持 Intel RealSense D455 相机和其他视频源。

## 功能特性

- ✅ 模块化设计，易于扩展新的视频源
- ✅ 支持 Intel RealSense D455 相机（彩色 + 深度流）
- ✅ 支持 USB/网络摄像头
- ✅ 支持视频文件和 RTSP 流
- ✅ 多线程架构，保证流畅性
- ✅ 时间戳叠加
- ✅ 命令行参数配置

## 架构设计

### 类层次结构

```
VideoSource (抽象基类)
├── RealSenseSource     (Intel RealSense 相机)
├── OpenCVSource        (USB 相机/文件/RTSP)
└── [自定义源...]       (可扩展)

WebRTCClient            (WebRTC 流客户端)
```

### 核心组件

1. **VideoSource**: 抽象基类，定义视频源接口
   - `initialize()`: 初始化视频源
   - `getFrame()`: 获取视频帧
   - `getWidth/Height/FrameRate()`: 获取视频参数
   - `release()`: 释放资源

2. **RealSenseSource**: RealSense 相机实现
   - 支持彩色流和深度流
   - 可配置分辨率和帧率
   - 线程安全的帧访问

3. **OpenCVSource**: OpenCV 视频源实现
   - 支持 USB 相机（设备 ID）
   - 支持视频文件
   - 支持 RTSP/RTMP 流

4. **WebRTCClient**: WebRTC 客户端
   - 视频采集线程
   - 信令处理线程
   - 帧缓冲队列

## 依赖项

### 必需

- **CMake** >= 3.10
- **C++ 编译器** 支持 C++17
- **OpenCV** >= 4.0
- **Intel RealSense SDK** >= 2.0 (如使用 RealSense 相机)

### 可选

- **WebRTC Native API** (用于完整的 WebRTC 功能)

## 编译

### 1. 安装依赖

#### Ubuntu/Debian

```bash
# 基础工具
sudo apt-get update
sudo apt-get install -y build-essential cmake git pkg-config

# OpenCV
sudo apt-get install -y libopencv-dev

# Intel RealSense SDK
sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-key F6E65AC044F831AC80A06380C8B3A55A6F3EFCDE
sudo add-apt-repository "deb https://librealsense.intel.com/Debian/apt-repo $(lsb_release -cs) main"
sudo apt-get update
sudo apt-get install -y librealsense2-dev
```

### 2. 编译项目

```bash
# 创建构建目录
mkdir build && cd build

# 配置
cmake ..

# 编译
make -j$(nproc)

# 安装（可选）
sudo make install
```

## 使用方法

### 基本用法

```bash
# 使用 RealSense 相机（默认）
./webrtc_streamer

# 使用 USB 摄像头
./webrtc_streamer --source camera --device 0

# 使用视频文件
./webrtc_streamer --source file --file /path/to/video.mp4

# 使用 RTSP 流
./webrtc_streamer --source rtsp --file rtsp://example.com/stream
```

### 高级选项

```bash
# RealSense 高分辨率 + 深度
./webrtc_streamer --source realsense --width 1280 --height 720 --fps 30 --depth

# 自定义服务器地址
./webrtc_streamer --server 192.168.1.100 --port 8080

# 完整示例
./webrtc_streamer \
    --source realsense \
    --width 1920 \
    --height 1080 \
    --fps 30 \
    --depth \
    --server 192.168.1.34 \
    --port 50061
```

### 命令行参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--source` | 视频源类型: realsense\|camera\|file\|rtsp | realsense |
| `--device` | 相机设备 ID | 0 |
| `--file` | 视频文件路径或 RTSP URL | - |
| `--width` | 视频宽度 | 640 |
| `--height` | 视频高度 | 480 |
| `--fps` | 帧率 | 30 |
| `--depth` | 启用深度流（仅 RealSense） | false |
| `--server` | 服务器 IP 地址 | 192.168.1.34 |
| `--port` | 服务器端口 | 50061 |
| `--help` | 显示帮助信息 | - |

## 扩展自定义视频源

### 示例：添加新的视频源

```cpp
// include/custom_source.h
#include "video_source.h"

class CustomSource : public VideoSource {
public:
    CustomSource(/* 参数 */);
    ~CustomSource() override;
    
    bool initialize() override;
    bool getFrame(cv::Mat& frame) override;
    int getWidth() const override { return width_; }
    int getHeight() const override { return height_; }
    int getFrameRate() const override { return fps_; }
    void release() override;
    std::string getName() const override { return "Custom Source"; }
    bool isReady() const override { return is_initialized_; }

private:
    int width_, height_, fps_;
    bool is_initialized_;
    // 自定义成员变量
};
```

```cpp
// src/custom_source.cpp
#include "custom_source.h"

bool CustomSource::initialize() {
    // 初始化逻辑
    is_initialized_ = true;
    return true;
}

bool CustomSource::getFrame(cv::Mat& frame) {
    // 获取帧逻辑
    return true;
}

void CustomSource::release() {
    // 清理资源
    is_initialized_ = false;
}
```

在 `main.cpp` 中添加：

```cpp
#include "custom_source.h"

// 在参数解析中添加
else if (source_type == "custom") {
    video_source = std::make_shared<CustomSource>(/* 参数 */);
}
```

## 与 Python 版本对比

| 功能 | Python (webrtc.py) | C++ |
|------|-------------------|-----|
| ROS2 集成 | ✅ | ❌ (可扩展) |
| RealSense 支持 | ❌ | ✅ |
| 性能 | 中等 | 高 |
| 内存占用 | 较高 | 低 |
| 扩展性 | 简单 | 灵活 |
| 部署 | 需要 Python 环境 | 独立可执行文件 |

## 集成 WebRTC Native API

当前实现是简化版本。要实现完整的 WebRTC 功能：

1. 下载并编译 WebRTC Native 代码
2. 修改 `CMakeLists.txt` 链接 WebRTC 库
3. 在 `webrtc_client.cpp` 中实现：
   - PeerConnection 创建
   - SDP 交换
   - ICE 候选处理
   - 视频轨道添加

参考：https://webrtc.github.io/webrtc-org/native-code/

## 故障排除

### RealSense 相机未检测到

```bash
# 检查设备
rs-enumerate-devices

# 更新固件
realsense-viewer
```

### OpenCV 无法打开摄像头

```bash
# 检查权限
sudo usermod -a -G video $USER

# 列出设备
v4l2-ctl --list-devices
```

### 编译错误

```bash
# 检查依赖
pkg-config --modversion opencv4
pkg-config --modversion realsense2
```

## 性能优化建议

1. **降低分辨率**：使用 640x480 而不是 1920x1080
2. **调整帧率**：30fps 通常足够，考虑使用 15fps
3. **启用硬件加速**：使用 GPU 编码
4. **优化队列大小**：根据网络条件调整 `MAX_QUEUE_SIZE`

## 许可证

参考主项目许可证

## 贡献

欢迎提交 Issue 和 Pull Request！

## 作者

基于原始 Python 实现改编
