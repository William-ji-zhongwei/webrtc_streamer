# WebRTC Streamer 配置文件说明

## 配置文件位置

默认配置文件：`config.json`

可通过命令行参数指定其他配置文件：
```bash
./webrtc_streamer --config /path/to/custom_config.json
```

## 配置项说明

### webrtc 部分

#### server
- `ip`: 信令服务器 IP 地址
- `port`: 信令服务器端口

#### ice_servers
ICE 服务器列表，用于 NAT 穿透。可包含多个 STUN/TURN 服务器。

##### STUN 服务器配置
```json
{
  "urls": ["stun:stun.example.com:19302"]
}
```

##### TURN 服务器配置
```json
{
  "urls": ["turn:turn.example.com:3478"],
  "username": "your_username",
  "credential": "your_password"
}
```

**常用公共 STUN 服务器：**
- `stun:stun.l.google.com:19302`
- `stun:stun1.l.google.com:19302`
- `stun:stun2.l.google.com:19302`
- `stun:stun.stunprotocol.org:3478`

**TURN 服务器：**
需要自己搭建或使用商业服务。推荐使用 [coturn](https://github.com/coturn/coturn)。

### video 部分

视频源配置（命令行参数优先级更高）：

- `source`: 视频源类型
  - `"realsense"`: Intel RealSense 相机
  - `"camera"`: USB/网络摄像头
  - `"file"`: 视频文件
  - `"rtsp"`: RTSP 流

- `width`: 视频宽度（像素）
- `height`: 视频高度（像素）
- `fps`: 帧率
- `device_id`: 相机设备 ID（仅 camera 源）
- `file_path`: 文件路径或 RTSP URL（仅 file/rtsp 源）
- `enable_depth`: 启用深度流（仅 RealSense）

### logging 部分

- `level`: 日志级别
  - `"debug"`: 调试信息
  - `"info"`: 一般信息
  - `"warning"`: 警告
  - `"error"`: 错误

- `enable_timestamp`: 是否在视频上叠加时间戳

## 配置示例

### 示例 1: 使用 RealSense 和自建 TURN 服务器

```json
{
  "webrtc": {
    "server": {
      "ip": "192.168.1.100",
      "port": 8080
    },
    "ice_servers": [
      {
        "urls": ["stun:stun.l.google.com:19302"]
      },
      {
        "urls": ["turn:192.168.1.100:3478"],
        "username": "webrtc_user",
        "credential": "secure_password_123"
      }
    ]
  },
  "video": {
    "source": "realsense",
    "width": 1280,
    "height": 720,
    "fps": 30,
    "enable_depth": true
  }
}
```

### 示例 2: 使用 USB 摄像头

```json
{
  "webrtc": {
    "server": {
      "ip": "example.com",
      "port": 50061
    },
    "ice_servers": [
      {
        "urls": ["stun:stun.l.google.com:19302"]
      }
    ]
  },
  "video": {
    "source": "camera",
    "width": 640,
    "height": 480,
    "fps": 30,
    "device_id": 0
  }
}
```

### 示例 3: 使用 RTSP 流

```json
{
  "webrtc": {
    "server": {
      "ip": "streaming.server.com",
      "port": 50061
    },
    "ice_servers": [
      {
        "urls": ["stun:stun.stunprotocol.org:3478"]
      },
      {
        "urls": ["turn:turn.example.com:3478"],
        "username": "user",
        "credential": "pass"
      }
    ]
  },
  "video": {
    "source": "rtsp",
    "file_path": "rtsp://camera.local:554/stream",
    "fps": 25
  }
}
```

## 搭建自己的 TURN 服务器

使用 coturn:

```bash
# 安装
sudo apt-get install coturn

# 配置 /etc/turnserver.conf
listening-port=3478
fingerprint
lt-cred-mech
user=username:password
realm=yourdomain.com
external-ip=YOUR_PUBLIC_IP

# 启动
sudo systemctl start coturn
sudo systemctl enable coturn
```

## 优先级

配置来源优先级（从高到低）：
1. 命令行参数
2. 配置文件
3. 默认值

例如：
```bash
# 使用配置文件中的设置，但覆盖视频源为 camera
./webrtc_streamer --config config.json --source camera
```
