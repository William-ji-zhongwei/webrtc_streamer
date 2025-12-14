# 简单 WebSocket 信令服务器

这是一个轻量级的 WebRTC 信令服务器，支持基于客户端 ID 的消息转发。

## 功能特性

- ✅ 客户端 ID 注册
- ✅ 点对点消息转发（指定 target_id）
- ✅ 广播模式（target_id 为空时）
- ✅ 自动连接管理

## 快速开始

### 安装依赖

```bash
pip3 install websockets
```

### 启动服务器

```bash
python3 server.py
```

服务器默认监听 `0.0.0.0:8765`

## 使用方法

### 1. 连接并注册

客户端连接后，第一条消息必须是注册：

```json
{
  "type": "register",
  "client_id": "sender_001"
}
```

服务器响应：

```json
{
  "type": "registered",
  "client_id": "sender_001"
}
```

### 2. 点对点发送（指定目标）

发送消息时添加 `target_id`：

```json
{
  "type": "offer",
  "sdp": "v=0...",
  "target_id": "receiver_001"
}
```

服务器会自动添加 `from` 字段并转发给 `receiver_001`：

```json
{
  "type": "offer",
  "sdp": "v=0...",
  "from": "sender_001"
}
```

### 3. 广播模式（不指定目标）

发送消息时不添加 `target_id`：

```json
{
  "type": "offer",
  "sdp": "v=0..."
}
```

服务器会广播给所有其他客户端（除了发送者）。

## 支持的消息类型

所有消息都会自动转发，常见类型：

- `offer` - WebRTC Offer
- `answer` - WebRTC Answer
- `candidate` - ICE Candidate

## 配置

如需修改监听地址或端口，编辑 `server.py`：

```python
host = "0.0.0.0"  # 改为特定 IP
port = 8765       # 改为其他端口
```

## 生产环境部署

### 使用 systemd

创建 `/etc/systemd/system/webrtc-signaling.service`：

```ini
[Unit]
Description=WebRTC Signaling Server
After=network.target

[Service]
Type=simple
User=your_user
WorkingDirectory=/path/to/signaling_server
ExecStart=/usr/bin/python3 server.py
Restart=always

[Install]
WantedBy=multi-user.target
```

启动：

```bash
sudo systemctl enable webrtc-signaling
sudo systemctl start webrtc-signaling
```

### 防火墙

```bash
sudo ufw allow 8765/tcp
```

## 测试

```bash
python3 test_server.py
```

或使用 `wscat`：

```bash
npm install -g wscat
wscat -c ws://localhost:8765
```

然后发送：
```json
{"type":"register","client_id":"test"}
```

## 日志

服务器输出日志格式：

```
INFO - ✅ Client registered: sender_001
INFO - 📤 Message sent to receiver_001: offer
INFO - ❌ Client unregistered: sender_001
```

## 架构说明

```
┌─────────────┐
│  Client A   │
│ (sender_001)│
└──────┬──────┘
       │
       ▼
┌─────────────────┐
│ Signaling Server│  (管理连接，转发消息)
│  (ID Router)    │
└─────────────────┘
       ▲
       │
┌──────┴──────┐
│  Client B   │
│(receiver_001)│
└─────────────┘
```

## 与 C++/Python 客户端配合

### C++ 发送端配置

```json
{
  "client_id": "sender_001",
  "target_id": "receiver_001"  // 或留空广播
}
```

### Python 接收端

```bash
python receiver_demo.py --client-id receiver_001
```

## 限制

- 仅处理信令，不传输媒体数据
- 无认证机制（生产环境需添加）
- 无消息持久化
