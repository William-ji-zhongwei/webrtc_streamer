# 房间式信令服务器部署和使用指南

## 概述

这是一个完整的房间式 WebRTC 信令解决方案，包括：
- Python WebSocket 信令服务器（支持多房间、多用户）
- C++ 视频发送端（支持 RealSense、OpenCV）
- Python 视频接收端（支持 H.264 解码）

## 架构说明

```
┌─────────────┐           ┌──────────────────┐           ┌─────────────┐
│  C++ Sender │◄─────────►│  Signaling       │◄─────────►│   Python    │
│  (Camera)   │  WebSocket │  Server (房间式)  │  WebSocket │  Receiver   │
└─────────────┘           └──────────────────┘           └─────────────┘
       │                                                         │
       └────────────────► WebRTC (P2P) ◄───────────────────────┘
                        (视频数据传输)
```

## 1. 信令服务器部署（云服务器）

### 1.1 上传文件

将 `signaling_server/` 目录上传到云服务器：

```bash
# 在本地
scp -r signaling_server/ user@106.14.31.123:/home/user/webrtc/
```

### 1.2 安装依赖

```bash
# SSH 登录云服务器
ssh user@106.14.31.123

cd /home/user/webrtc/signaling_server
pip3 install -r requirements.txt
```

### 1.3 配置防火墙

```bash
# 开放信令服务器端口
sudo ufw allow 8765/tcp

# 如果使用云服务商，还需在安全组中添加规则
# 入站：TCP 8765，源：0.0.0.0/0
```

### 1.4 使用 systemd 自动启动（推荐）

创建服务文件：

```bash
sudo nano /etc/systemd/system/webrtc-signaling.service
```

内容：

```ini
[Unit]
Description=WebRTC Room-based Signaling Server
After=network.target

[Service]
Type=simple
User=your_username
WorkingDirectory=/home/user/webrtc/signaling_server
ExecStart=/usr/bin/python3 /home/user/webrtc/signaling_server/server.py
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

启动服务：

```bash
sudo systemctl daemon-reload
sudo systemctl enable webrtc-signaling
sudo systemctl start webrtc-signaling

# 查看状态
sudo systemctl status webrtc-signaling

# 查看日志
sudo journalctl -u webrtc-signaling -f
```

### 1.5 手动运行（测试用）

```bash
cd signaling_server
chmod +x start.sh
./start.sh
```

## 2. C++ 发送端配置

### 2.1 修改配置文件

编辑 `config/config.json`：

```json
{
  "webrtc": {
    "server": {
      "ip": "106.14.31.123",        // 你的云服务器 IP
      "port": 8765
    },
    "client_id": "sender_camera_01",  // 唯一的发送端 ID
    "room_id": "living_room",         // 房间名称
    "role": "sender",
    "ice_servers": [
      {
        "urls": ["stun:106.14.31.123:3478"]
      },
      {
        "urls": ["turn:106.14.31.123:3478"],
        "username": "rxjqr",
        "credential": "rxjqrTurn123"
      }
    ]
  },
  "video": {
    "source": "camera",  // 或 "realsense"
    "width": 640,
    "height": 480,
    "fps": 30
  }
}
```

### 2.2 编译运行

```bash
cd /home/robdev/workspace/code/webrtc_streamer
./scripts/build.sh
./scripts/run.sh
```

## 3. Python 接收端运行

### 3.1 本地运行（笔记本等）

```bash
cd test
pip install aiortc opencv-python numpy websockets av

# 基本用法
python receiver_demo.py \
  --server-ip 106.14.31.123 \
  --server-port 8765 \
  --client-id receiver_laptop \
  --room-id living_room

# 多个接收端可以加入同一个房间
python receiver_demo.py \
  --server-ip 106.14.31.123 \
  --client-id receiver_phone \
  --room-id living_room
```

### 3.2 参数说明

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--server-ip` | 信令服务器 IP | 106.14.31.123 |
| `--server-port` | 信令服务器端口 | 8765 |
| `--client-id` | 客户端唯一 ID | receiver_001 |
| `--room-id` | 房间名称 | room_default |
| `--stun` | STUN 服务器 | 106.14.31.123:3478 |
| `--turn` | TURN 服务器 | 106.14.31.123:3478 |
| `--codec` | 视频编解码器 | h264 |

## 4. 使用场景

### 场景 1: 一对一视频传输

```bash
# 发送端配置
{
  "room_id": "room_001",
  "client_id": "sender_001"
}

# 接收端
python receiver_demo.py --room-id room_001 --client-id receiver_001
```

### 场景 2: 一对多广播

```bash
# 1 个发送端
sender_001 -> room_broadcast

# 多个接收端加入同一房间
python receiver_demo.py --room-id room_broadcast --client-id receiver_001
python receiver_demo.py --room-id room_broadcast --client-id receiver_002
python receiver_demo.py --room-id room_broadcast --client-id receiver_003
```

### 场景 3: 多房间隔离

```bash
# 房间 A
sender_A -> room_A <- receiver_A

# 房间 B（完全独立）
sender_B -> room_B <- receiver_B
```

## 5. 信令协议流程

### 5.1 发送端流程

```
1. 连接 WebSocket: ws://106.14.31.123:8765
2. 注册: {"type":"register", "client_id":"sender_001"}
3. 加入房间: {"type":"join", "room_id":"room_001", "role":"sender"}
4. 创建 PeerConnection
5. 创建 Offer
6. 发送 Offer: {"type":"offer", "sdp":"..."}
7. 接收 Answer: {"type":"answer", "sdp":"...", "from":"receiver_001"}
8. 交换 ICE Candidates
9. 开始视频传输
```

### 5.2 接收端流程

```
1. 连接 WebSocket: ws://106.14.31.123:8765
2. 注册: {"type":"register", "client_id":"receiver_001"}
3. 加入房间: {"type":"join", "room_id":"room_001", "role":"receiver"}
4. 创建 PeerConnection
5. 接收 Offer: {"type":"offer", "sdp":"...", "from":"sender_001"}
6. 创建 Answer
7. 发送 Answer: {"type":"answer", "sdp":"...", "target_id":"sender_001"}
8. 交换 ICE Candidates
9. 开始接收视频
```

## 6. 故障排查

### 6.1 信令服务器无法连接

```bash
# 检查服务器状态
sudo systemctl status webrtc-signaling

# 检查端口监听
sudo netstat -tulpn | grep 8765

# 检查防火墙
sudo ufw status
```

### 6.2 WebRTC 连接失败

```bash
# 检查 ICE 候选交换
# 发送端日志应显示: "ICE candidate sent"
# 接收端日志应显示: "收到 ICE candidate"

# 检查 STUN/TURN 服务器
# 确保 3478 端口开放
sudo ufw allow 3478/tcp
sudo ufw allow 3478/udp
```

### 6.3 视频无法显示

```bash
# 接收端：确保安装了 OpenGL 库
sudo apt-get install libgl1-mesa-glx libglib2.0-0

# 检查编解码器兼容性
# 发送端和接收端应使用相同的编解码器
```

## 7. 性能优化

### 7.1 信令服务器

```python
# server.py 可以添加性能监控
@self.pc.on("track")
async def on_track(track):
    logger.info(f"Bitrate: {track.get_stats()}")
```

### 7.2 视频编码

修改 C++ 端 `config.json`：

```json
{
  "video": {
    "width": 1280,    // 提高分辨率
    "height": 720,
    "fps": 60,        // 提高帧率
    "bitrate": 2000   // 提高码率 (需要代码支持)
  }
}
```

## 8. 安全建议

1. **使用 HTTPS/WSS**：生产环境应使用加密 WebSocket
2. **认证机制**：为信令服务器添加用户认证
3. **房间密码**：为敏感房间添加密码保护
4. **限流**：防止 DDoS 攻击

## 9. 监控和日志

### 9.1 查看信令服务器日志

```bash
# 实时日志
sudo journalctl -u webrtc-signaling -f

# 最近 100 行
sudo journalctl -u webrtc-signaling -n 100

# 特定时间
sudo journalctl -u webrtc-signaling --since "2025-12-14 10:00:00"
```

### 9.2 日志级别

修改 `server.py`：

```python
logging.basicConfig(
    level=logging.DEBUG,  # INFO, WARNING, ERROR
    format='%(asctime)s - %(levelname)s - %(message)s'
)
```

## 10. 常见问题

**Q: 如何更改房间？**
A: 修改配置文件中的 `room_id` 或使用命令行参数 `--room-id`

**Q: 支持多少个用户？**
A: 取决于服务器性能。一般单台服务器可支持 100+ 房间，每个房间几十个用户。

**Q: 视频延迟多少？**
A: 通常 200-500ms，取决于网络条件和 TURN 服务器性能。

**Q: 如何录制视频？**
A: 接收端可以添加 cv2.VideoWriter 保存视频流。
