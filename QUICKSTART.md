# 快速开始指南

## 简单版信令服务器

这是一个简化的信令服务器，支持：
- ✅ 基于客户端 ID 的点对点转发
- ✅ 广播模式（target_id 为空时）
- ✅ 自动路由消息给目标客户端

## 5 分钟快速测试

### Step 1: 启动信令服务器（云服务器或本地）

```bash
cd signaling_server
pip3 install websockets
python3 server.py
```

看到以下输出表示成功：
```
INFO - Starting signaling server on 0.0.0.0:8765
INFO - ✅ Signaling server is running on ws://0.0.0.0:8765
```

### Step 2: 配置 C++ 发送端

编辑 `config/config.json`：

```json
{
  "webrtc": {
    "server": {
      "ip": "127.0.0.1",      // 本地测试用，云端改为公网 IP
      "port": 8765
    },
    "client_id": "sender_001",
    "target_id": "receiver_001"  // 指定接收方 ID，留空则广播
  }
}
```

**重要参数说明：**
- `client_id`: 自己的 ID（必填）
- `target_id`: 接收方的 ID
  - **留空** = 广播给所有其他客户端
  - **指定 ID** = 只发给这个接收方（例如 "receiver_001"）

### Step 3: 启动 C++ 发送端

```bash
./scripts/build.sh
./scripts/run.sh
```

### Step 4: 启动 Python 接收端

新开一个终端：

```bash
cd test

# 方式 1: 指定接收方 ID（与发送端的 target_id 匹配）
python receiver_demo.py \
  --server-ip 127.0.0.1 \
  --client-id receiver_001

# 方式 2: 多个接收端（如果发送端 target_id 为空）
python receiver_demo.py --client-id receiver_002
python receiver_demo.py --client-id receiver_003
```

### Step 5: 观看视频

**发送端日志：**
```
✅ WebSocket connected
📤 Registered as: sender_001
📥 Server response: {"type":"registered","client_id":"sender_001"}
📤 Sending offer to: receiver_001    # 或 "Broadcasting offer"
✅ Answer received and set
```

**接收端日志：**
```
✅ 已连接到信令服务器
✅ 已注册: receiver_001
📥 收到来自 sender_001 的 offer
📤 已发送 answer 到 sender_001
✅ ICE 连接已建立，视频流应该开始传输
📹 接收到媒体轨道: video
```

## 控制视频发送目标

### 场景 1: 一对一（指定接收方）

**发送端配置:**
```json
{
  "client_id": "sender_001",
  "target_id": "receiver_laptop"  // 只发给这个接收方
}
```

**接收端:**
```bash
python receiver_demo.py --client-id receiver_laptop
```

### 场景 2: 一对多广播

**发送端配置:**
```json
{
  "client_id": "sender_001",
  "target_id": ""  // 留空 = 广播
}
```

**多个接收端:**
```bash
python receiver_demo.py --client-id receiver_001
python receiver_demo.py --client-id receiver_002
python receiver_demo.py --client-id receiver_003
```

### 场景 3: 动态切换接收方

修改配置文件的 `target_id` 然后重启发送端：

```bash
# 发给 receiver_A
"target_id": "receiver_A"

# 改为发给 receiver_B
"target_id": "receiver_B"
```

## 云服务器部署

### 1. 部署信令服务器

```bash
# SSH 登录云服务器
ssh user@106.14.31.123

# 创建目录
mkdir -p ~/webrtc/signaling_server
cd ~/webrtc/signaling_server

# 上传 server.py（或使用 git clone）
# 然后安装依赖
pip3 install websockets

# 使用 systemd 自启动（推荐）
sudo nano /etc/systemd/system/webrtc-signaling.service
```

**服务文件内容：**
```ini
[Unit]
Description=WebRTC Signaling Server
After=network.target

[Service]
Type=simple
User=ubuntu
WorkingDirectory=/home/ubuntu/webrtc/signaling_server
ExecStart=/usr/bin/python3 server.py
Restart=always

[Install]
WantedBy=multi-user.target
```

**启动服务：**
```bash
sudo systemctl enable webrtc-signaling
sudo systemctl start webrtc-signaling
sudo systemctl status webrtc-signaling
```

### 2. 开放端口

```bash
sudo ufw allow 8765/tcp
```

### 3. 配置发送端

修改 `config/config.json`：
```json
{
  "server": {
    "ip": "106.14.31.123",  // 云服务器公网 IP
    "port": 8765
  },
  "client_id": "sender_camera",
  "target_id": "receiver_laptop"
}
```

### 4. 接收端从任何地方连接

```bash
python receiver_demo.py \
  --server-ip 106.14.31.123 \
  --client-id receiver_laptop
```

## 常用命令

### 查看服务器状态
```bash
sudo systemctl status webrtc-signaling
```

### 查看实时日志
```bash
sudo journalctl -u webrtc-signaling -f
```

### 测试服务器连接
```bash
# 安装 wscat
npm install -g wscat

# 测试连接
wscat -c ws://106.14.31.123:8765

# 发送注册消息
{"type":"register","client_id":"test"}
```

## 故障排查

### 问题 1: 连接失败

**检查：**
```bash
# 服务器是否运行
ps aux | grep server.py

# 端口是否监听
netstat -tulpn | grep 8765

# 防火墙
sudo ufw status
```

### 问题 2: 收不到 offer

**可能原因：**
- `target_id` 拼写错误
- 接收端的 `client_id` 与 `target_id` 不匹配
- 接收端还未连接

**解决：**
1. 确保接收端先启动并注册
2. 检查 ID 是否匹配
3. 查看服务器日志确认注册状态

### 问题 3: ICE 连接失败

**检查：**
- STUN/TURN 服务器是否可访问
- 防火墙是否开放 3478 端口
- 网络类型（某些严格 NAT 需要 TURN）

## 协议说明

### 消息格式

**1. 注册:**
```json
{"type": "register", "client_id": "sender_001"}
```

**2. Offer (点对点):**
```json
{
  "type": "offer",
  "sdp": "v=0...",
  "target_id": "receiver_001"
}
```

**3. Offer (广播):**
```json
{
  "type": "offer",
  "sdp": "v=0..."
  // 没有 target_id
}
```

**4. Answer:**
```json
{
  "type": "answer",
  "sdp": "v=0...",
  "target_id": "sender_001",
  "from": "receiver_001"
}
```

## 下一步

现在你已经可以：
1. ✅ 指定特定接收方发送视频
2. ✅ 广播视频给所有接收端
3. ✅ 部署到云服务器供远程访问

更多高级功能请参考完整文档。
