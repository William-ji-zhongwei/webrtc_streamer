# WebRTC 接收端使用指南

本目录提供了两种接收端实现方式。

## 1. Python 接收端（推荐）

### 安装依赖

```bash
pip install aiortc opencv-python numpy av
```

### 运行接收端

```bash
# 使用默认配置
python receiver_demo.py

# 自定义配置
python receiver_demo.py --ip 0.0.0.0 --port 50061 --stun 106.14.31.123:3478 --turn 106.14.31.123:3478 --turn-user webrtc --turn-pass webrtc
```

### 参数说明

- `--ip`: 信令服务器监听 IP（0.0.0.0 表示监听所有网络接口）
- `--port`: 信令服务器端口（默认: 50061）
- `--stun`: STUN 服务器地址（默认: 106.14.31.123:3478）
- `--turn`: TURN 服务器地址（默认: 106.14.31.123:3478）
- `--turn-user`: TURN 用户名（默认: webrtc）
- `--turn-pass`: TURN 密码（默认: webrtc）

### 使用流程

1. **启动接收端**
   ```bash
   python receiver_demo.py
   ```
   接收端会监听在 0.0.0.0:50061，等待发送端连接。

2. **启动发送端（C++）**
   ```bash
   ./build/webrtc_streamer --source camera --device 0
   ```
   发送端会读取 config.json 配置并连接到接收端。

3. **查看视频流**
   - 连接成功后会自动弹出 OpenCV 窗口显示视频
   - 按 'q' 键退出

## 2. Web 接收端（浏览器版）

### 特点

- 无需安装依赖
- 可以在浏览器中直接运行
- 需要 WebSocket 信令服务器（不同于 TCP Socket）

### 使用方法

1. 直接在浏览器中打开 `receiver_web.html`
2. 配置连接参数
3. 点击"连接并接收视频"

⚠️ **注意**: Web 版需要 WebSocket 信令服务器支持，与当前的 TCP Socket 信令不兼容。建议使用 Python 版本。

## 完整测试流程

### 场景 1: 本地测试

**接收端（在机器 A）:**
```bash
python receiver_demo.py --ip 0.0.0.0 --port 50061
```

**发送端（在机器 A 或 B）:**
```bash
# 编辑 config.json，设置 server.ip 为接收端的 IP
./build/webrtc_streamer --source camera --device 0
```

### 场景 2: 远程测试（通过 TURN）

**配置文件 (config.json):**
```json
{
  "webrtc": {
    "server": {
      "ip": "106.14.31.123",
      "port": 50061
    },
    "ice_servers": [
      {
        "urls": ["stun:106.14.31.123:3478"]
      },
      {
        "urls": ["turn:106.14.31.123:3478"],
        "username": "webrtc",
        "credential": "webrtc"
      }
    ]
  }
}
```

**接收端（在服务器 106.14.31.123）:**
```bash
python receiver_demo.py --ip 0.0.0.0 --port 50061
```

**发送端（在任意位置）:**
```bash
./build/webrtc_streamer
```

## 故障排除

### 连接不上

1. **检查防火墙**
   ```bash
   # 接收端服务器需要开放端口
   sudo ufw allow 50061/tcp    # 信令端口
   sudo ufw allow 3478/udp     # STUN/TURN 端口
   sudo ufw allow 49152:65535/udp  # WebRTC 媒体端口范围
   ```

2. **检查 STUN/TURN 服务器**
   ```bash
   # 测试 STUN
   stunclient 106.14.31.123 3478
   
   # 检查 TURN 服务
   turnutils_uclient -v -u webrtc -w webrtc 106.14.31.123
   ```

3. **查看日志**
   - 接收端会输出详细的连接日志
   - 检查 ICE 连接状态和候选信息

### 视频不显示

1. 检查摄像头是否正常工作
2. 确认防火墙允许 UDP 流量
3. 检查 TURN 服务器认证信息是否正确

### 延迟过高

1. 使用就近的 STUN/TURN 服务器
2. 检查网络带宽
3. 降低视频分辨率和帧率

## 性能优化

### 推荐配置

**低延迟（本地网络）:**
```json
{
  "video": {
    "width": 640,
    "height": 480,
    "fps": 30
  }
}
```

**高质量（良好网络）:**
```json
{
  "video": {
    "width": 1280,
    "height": 720,
    "fps": 30
  }
}
```

**低带宽（移动网络）:**
```json
{
  "video": {
    "width": 320,
    "height": 240,
    "fps": 15
  }
}
```

## 常见问题

**Q: 为什么 Web 版不能直接用？**
A: Web 版使用 WebSocket 信令，而 C++ 客户端使用 TCP Socket。需要实现 WebSocket 到 TCP 的转换网关。

**Q: 可以同时连接多个接收端吗？**
A: 当前实现是点对点连接，如需多接收端，需要实现 SFU/MCU 服务器。

**Q: 视频卡顿怎么办？**
A: 
1. 降低分辨率和帧率
2. 检查网络质量
3. 确保使用 TURN 服务器（如果在 NAT 后面）

## 下一步

- 实现多路视频流
- 添加音频支持
- 实现录制功能
- 添加 WebSocket 信令支持
