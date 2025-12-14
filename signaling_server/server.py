#!/usr/bin/env python3
"""
简单的 WebSocket 信令服务器
支持指定目标 ID 转发消息
"""

import asyncio
import websockets
import json
import logging
from typing import Dict

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

class SignalingServer:
    """简单信令服务器 - 基于客户端 ID 转发"""
    
    def __init__(self):
        # 客户端 ID -> WebSocket 连接
        self.clients: Dict[str, websockets.WebSocketServerProtocol] = {}
    
    async def register(self, websocket: websockets.WebSocketServerProtocol, client_id: str):
        """注册客户端"""
        self.clients[client_id] = websocket
        logger.info(f"✅ Client registered: {client_id}")
        return client_id
    
    async def unregister(self, client_id: str):
        """注销客户端"""
        if client_id in self.clients:
            del self.clients[client_id]
            logger.info(f"❌ Client unregistered: {client_id}")
    
    async def send_to_client(self, target_id: str, message: dict):
        """发送消息给指定客户端"""
        if target_id not in self.clients:
            logger.warning(f"⚠️  Target not found: {target_id}")
            return False
        
        try:
            await self.clients[target_id].send(json.dumps(message))
            logger.info(f"📤 Message sent to {target_id}: {message.get('type')}")
            return True
        except Exception as e:
            logger.error(f"❌ Failed to send to {target_id}: {e}")
            return False
    
    async def broadcast(self, message: dict, exclude_id: str = None):
        """广播消息给所有客户端（可排除某个）"""
        message_str = json.dumps(message)
        tasks = []
        
        for client_id, websocket in self.clients.items():
            if client_id != exclude_id:
                tasks.append(websocket.send(message_str))
        
        if tasks:
            await asyncio.gather(*tasks, return_exceptions=True)
            logger.info(f"📢 Broadcast to {len(tasks)} clients")
    
    async def handle_message(self, from_id: str, message: dict):
        """处理客户端消息"""
        msg_type = message.get("type")
        target_id = message.get("target_id")
        
        # 添加发送者信息
        message["from"] = from_id
        
        if target_id:
            # 点对点转发
            await self.send_to_client(target_id, message)
        else:
            # 广播给所有其他客户端
            await self.broadcast(message, exclude_id=from_id)
    
    async def handler(self, websocket: websockets.WebSocketServerProtocol, path: str):
        """WebSocket 连接处理器"""
        client_id = None
        
        try:
            # 第一条消息必须是注册
            register_msg = await websocket.recv()
            register_data = json.loads(register_msg)
            
            if register_data.get("type") != "register":
                await websocket.send(json.dumps({
                    "type": "error",
                    "message": "First message must be 'register' with client_id"
                }))
                return
            
            client_id = register_data.get("client_id", f"client_{id(websocket)}")
            await self.register(websocket, client_id)
            
            # 确认注册
            await websocket.send(json.dumps({
                "type": "registered",
                "client_id": client_id
            }))
            
            # 处理后续消息
            async for message in websocket:
                try:
                    data = json.loads(message)
                    await self.handle_message(client_id, data)
                except json.JSONDecodeError:
                    logger.error(f"Invalid JSON from {client_id}")
                except Exception as e:
                    logger.error(f"Error handling message: {e}")
        
        except websockets.exceptions.ConnectionClosed:
            logger.info(f"Connection closed for {client_id}")
        except Exception as e:
            logger.error(f"Handler error: {e}")
        finally:
            if client_id:
                await self.unregister(client_id)

async def main():
    """启动信令服务器"""
    server = SignalingServer()
    
    host = "0.0.0.0"  # 监听所有网络接口
    port = 50061
    
    logger.info(f"Starting signaling server on {host}:{port}")
    
    async with websockets.serve(server.handler, host, port):
        logger.info(f"✅ Signaling server is running on ws://{host}:{port}")
        await asyncio.Future()  # 永久运行

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("Server stopped by user")
