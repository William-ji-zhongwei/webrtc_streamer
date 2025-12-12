# WebRTC C++ Streamer

基于 WebRTC 原生 API 的高性能 C++ 视频流客户端，支持多种视频源和 H.265 编解码。

## ✨ 功能特性

- 🌐 **原生 WebRTC API**: 使用社区预编译库，完整 WebRTC 功能支持
- 📹 **多种视频源**: Intel RealSense D455、USB 摄像头、视频文件、RTSP 流
- 🚀 **高性能编解码**: H.265/HEVC 硬件加速支持
- 🔧 **模块化设计**: 易于扩展新的视频源和编码器
- ⚡ **多线程架构**: 视频采集、编码、传输独立线程
- 🎯 **低延迟**: <100ms 端到端延迟

## 📋 系统要求

### 必需依赖

- **CMake** >= 3.10
- **C++17** 编译器 (GCC 7+, Clang 5+)
- **OpenCV** >= 4.0
- **WebRTC** 预编译库 (M100+)
- **FFmpeg** (H.265 解码: libavcodec, libavformat, libavutil, libswscale)
- **x265** (H.265 编码)

### 可选依赖

- **Intel RealSense SDK** >= 2.0 (使用 RealSense 相机时)

---

## 🚀 快速开始

### 一键安装（推荐）

```bash
chmod +x scripts/setup_all.sh
./scripts/setup_all.sh
```

### 分步安装

#### 1. 安装 WebRTC 预编译库

```bash
chmod +x scripts/*.sh
sudo ./scripts/install_webrtc.sh
```

**🇨🇳 国内用户**: 脚本已内置国内镜像加速（ghproxy、Gitee、清华镜像）

如无法下载，请使用代理：
```bash
export http_proxy=http://127.0.0.1:7890
export https_proxy=http://127.0.0.1:7890
sudo -E ./scripts/install_webrtc.sh
```

或从网盘手动下载后放到 `/tmp/webrtc-install/webrtc.tar.gz`

#### 2. 安装其他依赖

```bash
# 自动安装
sudo ./scripts/install_webrtc_deps.sh

# 或手动安装
sudo apt-get update
sudo apt-get install -y build-essential cmake git pkg-config \
    libopencv-dev libavcodec-dev libavformat-dev libavutil-dev \
    libswscale-dev libx265-dev x265

# RealSense SDK (可选)
sudo ./scripts/install_realsense.sh
```

#### 3. 编译

```bash
./scripts/build.sh          # Release 构建
./scripts/build.sh Debug    # Debug 构建
```

#### 4. 运行

```bash
./build/webrtc_streamer --help
./build/webrtc_streamer --source camera --device 0
```

---

## 📖 使用指南

### 基本用法

```bash
# USB 摄像头
./build/webrtc_streamer --source camera --device 0

# RealSense 相机
./build/webrtc_streamer --source realsense --width 1280 --height 720

# 视频文件
./build/webrtc_streamer --source file --file video.mp4

# RTSP 流
./build/webrtc_streamer --source rtsp --file rtsp://192.168.1.100/stream
```

### 命令行参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--source` | 视频源: `realsense`\|`camera`\|`file`\|`rtsp` | `realsense` |
| `--device` | 摄像头设备 ID | `0` |
| `--file` | 文件路径或 RTSP URL | - |
| `--width` | 视频宽度 | `640` |
| `--height` | 视频高度 | `480` |
| `--fps` | 帧率 | `30` |
| `--depth` | 启用深度流（RealSense） | `false` |
| `--server` | 服务器 IP | `192.168.1.34` |
| `--port` | 服务器端口 | `50061` |

### 配置文件

编辑 `config/config.json`:

```json
{
  "video": {
    "source": "camera",
    "width": 1280,
    "height": 720,
    "fps": 30,
    "codec": "h264",
    "bitrate_kbps": 2000
  },
  "webrtc": {
    "server_ip": "192.168.1.34",
    "server_port": 50061,
    "ice_servers": [
      { "urls": ["stun:stun.l.google.com:19302"] },
      {
        "urls": ["turn:your-turn-server:3478"],
        "username": "user",
        "credential": "pass"
      }
    ]
  }
}
```

---

## 🏗️ 架构设计

### 核心组件

```
VideoSource (抽象基类)
├── RealSenseSource     - Intel RealSense 相机
├── OpenCVSource        - USB/文件/RTSP
└── CustomVideoSource   - OpenCV → WebRTC 适配器

WebRTCClient (原生 API)
├── PeerConnectionFactory
├── PeerConnection
├── VideoTrackSource
└── Observers (ICE/Signaling)

```

### 数据流

```
Camera/File → VideoSource → CustomVideoSource
                                  ↓
                          H.265 Encoder (x265)
                                  ↓
                          WebRTC PeerConnection
                                  ↓
                          Network (STUN/TURN)
                                  ↓
                          Python Receiver
                                  ↓
                          H.265 Decoder (FFmpeg)
                                  ↓
                          Display
```

---

## 🔧 WebRTC 原生 API 说明

项目使用完整的 WebRTC 原生 C++ API：

### 关键头文件

```cpp
#include <api/create_peerconnection_factory.h>
#include <api/peer_connection_interface.h>
#include <media/base/adapted_video_track_source.h>
#include <api/video/i420_buffer.h>
#include <rtc_base/ssl_adapter.h>
```

### PeerConnection 创建

```cpp
peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
    network_thread.get(), worker_thread.get(), signaling_thread.get(),
    nullptr,
    webrtc::CreateBuiltinAudioEncoderFactory(),
    webrtc::CreateBuiltinAudioDecoderFactory(),
    webrtc::CreateBuiltinVideoEncoderFactory(),
    webrtc::CreateBuiltinVideoDecoderFactory(),
    nullptr, nullptr
);
```

### 自定义视频源

```cpp
class CustomVideoSource : public rtc::AdaptedVideoTrackSource {
public:
    void PushFrame(const cv::Mat& frame) {
        auto buffer = webrtc::I420Buffer::Create(width, height);
        libyuv::RGB24ToI420(...);  // BGR → I420
        OnFrame(webrtc::VideoFrame::Builder()
            .set_video_frame_buffer(buffer)
            .set_timestamp_us(timestamp_us_)
            .build());
    }
};
```

### WebRTC 库位置

- **安装路径**: `/opt/webrtc`
- **支持结构**: 
  - 标准预编译: `include/` + `lib/`
  - 源码编译: `src/` + `out/Default/`

---

```

**优化参数**:
- Preset: `medium` (平衡质量/速度)
- Tune: `zerolatency` (低延迟)
- B-frames: `0` (无 B 帧)
- Rate control: ABR

```

### 性能基准

| 分辨率 | CPU 使用率 | 内存 | 延迟 | 码率 |
|--------|-----------|------|------|------|
| 640×480@30fps | ~40% | 150MB | <100ms | 2Mbps |
| 1280×720@30fps | ~70% | 200MB | <150ms | 4Mbps |
| 1920×1080@30fps | ~90% | 300MB | <200ms | 6Mbps |

---

## 🛠️ 开发指南

### 添加自定义视频源

```cpp
// include/my_source.h
class MySource : public VideoSource {
public:
    bool initialize() override;
    bool getFrame(cv::Mat& frame) override;
    void release() override;
    std::string getName() const override { return "My Source"; }
};

// src/my_source.cpp
bool MySource::getFrame(cv::Mat& frame) {
    // 你的采集逻辑
    return true;
}

// src/main.cpp 中注册
if (source_type == "mysource") {
    video_source = std::make_shared<MySource>();
}
```

### CMake 选项

```bash
cmake .. \
  -DUSE_NATIVE_WEBRTC=ON \           # 使用原生 WebRTC
  -DWEBRTC_ROOT_DIR=/opt/webrtc \    # WebRTC 路径
  -DENABLE_REALSENSE=OFF \           # RealSense 支持
  -DCMAKE_BUILD_TYPE=Release         # 构建类型
```

---

## 🐛 故障排除

### WebRTC 找不到

```bash
# 检查安装
ls -la /opt/webrtc

# 重新安装
sudo rm -rf /opt/webrtc
sudo ./scripts/install_webrtc.sh
```

### 编译错误

```bash
# 找不到头文件
export WEBRTC_ROOT_DIR=/opt/webrtc
cmake .. -DWEBRTC_ROOT_DIR=/opt/webrtc

# 清理重建
./scripts/build.sh Release clean
```

### RealSense 未检测到

```bash
# 检查设备
rs-enumerate-devices

# 权限问题
sudo usermod -a -G video $USER
```

### H.265 编码失败

```bash
# 检查 x265
pkg-config --modversion x265

# 重新安装
sudo apt-get install --reinstall libx265-dev
```

---

## 📊 性能优化

1. **降低分辨率**: 640×480 而非 1920×1080
2. **调整帧率**: 15-30 fps
3. **x265 preset**: `ultrafast` (速度优先) 或 `medium` (质量优先)
4. **码率控制**: 2Mbps (720p), 4Mbps (1080p)
5. **多线程**: x265 自动使用多核

---

## 📁 项目结构

```
webrtc-streamer/
├── include/              # 头文件
│   ├── webrtc_client.h
│   ├── custom_video_source.h
│   ├── video_source.h
│   └── ...
├── src/                  # 源文件
│   ├── webrtc_client.cpp    # WebRTC 原生实现
│   ├── custom_video_source.cpp
│   ├── main.cpp
│   └── ...
├── scripts/              # 安装/构建脚本
│   ├── install_webrtc.sh
│   ├── install_webrtc_deps.sh
│   ├── build.sh
│   └── setup_all.sh
├── CMakeLists.txt        # CMake 配置
├── config/               # 配置文件目录
│   └── config.json      # 运行时配置
└── README.md
```

---

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

## 📄 许可证

参考主项目许可证

## 🔗 相关资源

- **WebRTC 官方**: https://webrtc.org/
- **WebRTC Native API**: https://webrtc.googlesource.com/src/+/refs/heads/main/docs/native-code/
- **x265 文档**: https://x265.readthedocs.io/
- **FFmpeg**: https://ffmpeg.org/
