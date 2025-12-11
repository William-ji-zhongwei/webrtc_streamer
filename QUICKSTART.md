# 快速启动指南

## 问题解决

### RealSense 依赖错误

如果编译时遇到以下错误：
```
The following imported targets are referenced, but are missing: fastcdr fastrtps
```

**解决方案：**

#### 方案 1：不使用 RealSense（推荐快速开始）

项目已支持不依赖 RealSense 编译，仍可使用 USB 摄像头、视频文件等其他视频源：

```bash
# 直接编译（自动跳过 RealSense）
./scripts/build.sh

# 使用 USB 摄像头运行
./build/webrtc_streamer --source camera --device 0
```

#### 方案 2：安装完整 RealSense 支持

运行专门的 RealSense 安装脚本：

```bash
# 安装 Fast-DDS 和 RealSense（需要较长时间）
./scripts/install_realsense.sh

# 重新编译以启用 RealSense
./scripts/build.sh clean
./scripts/build.sh
```

## 编译状态说明

编译时会看到以下消息之一：

### ✅ 有 RealSense 支持
```
-- Found RealSense: 2.x.x
```

### ⚠️ 无 RealSense 支持（仍可正常使用）
```
CMake Warning: RealSense not found, building without RealSense support
```
这是正常的，可以继续使用其他视频源。

## 快速测试

### 测试 USB 摄像头
```bash
./build/webrtc_streamer --source camera --device 0
```

### 测试视频文件
```bash
./build/webrtc_streamer --source file --file /path/to/video.mp4
```

### 测试 RTSP 流
```bash
./build/webrtc_streamer --source rtsp --file rtsp://example.com/stream
```

## 完整文档

详细文档请查看 [README.md](README.md)
