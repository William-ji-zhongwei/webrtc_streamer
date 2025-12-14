#!/usr/bin/env python3
"""
快速测试脚本 - 测试信令服务器是否正常工作
"""

import asyncio
import websockets
import json

async def test_signaling():
    uri = "ws://localhost:8765"
    print(f"连接到信令服务器: {uri}")
    
    try:
        async with websockets.connect(uri) as websocket:
            print("✅ WebSocket 连接成功")
            
            # 注册
            await websocket.send(json.dumps({
                "type": "register",
                "client_id": "test_client"
            }))
            response = await websocket.recv()
            print(f"注册响应: {response}")
            
            # 加入房间
            await websocket.send(json.dumps({
                "type": "join",
                "room_id": "test_room",
                "role": "sender"
            }))
            response = await websocket.recv()
            print(f"加入房间响应: {response}")
            
            print("✅ 信令服务器测试成功！")
            
    except Exception as e:
        print(f"❌ 测试失败: {e}")

if __name__ == "__main__":
    print("=" * 60)
    print("信令服务器测试")
    print("=" * 60)
    asyncio.run(test_signaling())
