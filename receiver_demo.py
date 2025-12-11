"""
WebRTC 接收端 Demo - Python 版本
用于接收来自 C++ 客户端的视频流
支持 H.265/HEVC 解码

依赖安装：
pip install aiortc opencv-python numpy websockets av

运行方式：
python receiver_demo.py
"""

import asyncio
import cv2
import numpy as np
import json
from aiortc import RTCPeerConnection, RTCSessionDescription, RTCIceCandidate, VideoStreamTrack
import websockets
from av import VideoFrame, CodecContext
import argparse
import logging

# 配置日志
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class VideoReceiver:
    def __init__(self, server_ip="0.0.0.0", server_port=50061, 
                 stun_server="106.14.31.123:3478",
                 turn_server="106.14.31.123:3478",
                 turn_username="rxjqr",
                 turn_password="rxjqrTurn123",
                 codec="h265"):
        """
        初始化视频接收器
        
        Args:
            server_ip: 信令服务器监听 IP（0.0.0.0 表示监听所有接口）
            server_port: 信令服务器端口
            stun_server: STUN 服务器地址
            turn_server: TURN 服务器地址
            turn_username: TURN 用户名
            turn_password: TURN 密码
            codec: 视频编解码器 (h264/h265/vp8/vp9)
        """
        self.server_ip = server_ip
        self.server_port = server_port
        self.codec = codec.lower()
        
        # 配置 ICE 服务器
        self.ice_servers = [
            {"urls": [f"stun:{stun_server}"]},
            {
                "urls": [f"turn:{turn_server}"],
                "username": turn_username,
                "credential": turn_password
            }
        ]
        
        self.pc = None
        self.ws = None
        self.video_track = None
        self.frame_count = 0
        self.running = False
        
        logger.info(f"使用编解码器: {self.codec.upper()}")

    async def receive_frames(self):
        """接收并显示视频帧（支持 H.265 解码）"""
        logger.info("开始接收视频流...")
        
        try:
            while self.running:
                if self.video_track:
                    try:
                        frame = await asyncio.wait_for(
                            self.video_track.recv(), 
                            timeout=1.0
                        )
                        
                        # 转换为 numpy 数组
                        img = frame.to_ndarray(format="bgr24")
                        
                        self.frame_count += 1
                        
                        # 添加帧信息
                        codec_info = f"Codec: {self.codec.upper()}"
                        cv2.putText(
                            img, 
                            codec_info,
                            (10, 30),
                            cv2.FONT_HERSHEY_SIMPLEX,
                            0.7,
                            (0, 255, 255),
                            2
                        )
                        
                        cv2.putText(
                            img, 
                            f"Frame: {self.frame_count}", 
                            (10, 60), 
                            cv2.FONT_HERSHEY_SIMPLEX, 
                            0.7, 
                            (0, 255, 0), 
                            2
                        )
                        
                        # 显示视频
                        cv2.imshow(f'WebRTC Receiver ({self.codec.upper()})', img)
                        
                        # 按 'q' 退出
                        if cv2.waitKey(1) & 0xFF == ord('q'):
                            logger.info("用户请求退出")
                            self.running = False
                            break
                            
                        if self.frame_count % 30 == 0:
                            logger.info(f"已接收 {self.frame_count} 帧 ({self.codec.upper()})")
                            
                    except asyncio.TimeoutError:
                        continue
                    except Exception as e:
                        logger.error(f"接收帧错误: {e}")
                        break
                else:
                    await asyncio.sleep(0.1)
                    
        finally:
            cv2.destroyAllWindows()

    async def handle_websocket_message(self, message):
        """处理 WebSocket 消息"""
        data = json.loads(message)
        msg_type = data.get('type')
        
        if msg_type == 'offer':
            logger.info("收到 offer")
            # 设置远端描述
            offer = RTCSessionDescription(sdp=data['sdp'], type='offer')
            await self.pc.setRemoteDescription(offer)
            
            # 创建 answer
            answer = await self.pc.createAnswer()
            await self.pc.setLocalDescription(answer)
            
            # 发送 answer
            await self.ws.send(json.dumps({
                'type': 'answer',
                'sdp': self.pc.localDescription.sdp
            }))
            logger.info("已发送 answer")
            
        elif msg_type == 'candidate':
            logger.info("收到 ICE candidate")
            candidate_data = data['candidate']
            candidate = RTCIceCandidate(
                candidate=candidate_data['candidate'],
                sdpMid=candidate_data.get('sdpMid'),
                sdpMLineIndex=candidate_data.get('sdpMLineIndex')
            )
            await self.pc.addIceCandidate(candidate)

    async def run(self):
        """运行接收器"""
        self.running = True
        
        # 创建 PeerConnection
        self.pc = RTCPeerConnection(
            configuration={"iceServers": self.ice_servers}
        )
        
        logger.info(f"WebSocket 信令服务器监听于 ws://{self.server_ip}:{self.server_port}")
        logger.info("等待客户端连接...")
        
        @self.pc.on("track")
        async def on_track(track):
            logger.info(f"接收到媒体轨道: {track.kind}")
            if track.kind == "video":
                self.video_track = track
                logger.info("视频轨道已就绪")

        @self.pc.on("connectionstatechange")
        async def on_connectionstatechange():
            logger.info(f"连接状态: {self.pc.connectionState}")
            if self.pc.connectionState == "connected":
                logger.info("WebRTC 连接成功建立！")
            elif self.pc.connectionState == "failed":
                logger.error("WebRTC 连接失败")
                self.running = False
            elif self.pc.connectionState == "closed":
                logger.info("WebRTC 连接已关闭")
                self.running = False

        @self.pc.on("iceconnectionstatechange")
        async def on_iceconnectionstatechange():
            logger.info(f"ICE 连接状态: {self.pc.iceConnectionState}")
        
        @self.pc.on("icecandidate")
        async def on_icecandidate(candidate):
            if candidate and self.ws:
                await self.ws.send(json.dumps({
                    'type': 'candidate',
                    'candidate': {
                        'candidate': candidate.candidate,
                        'sdpMid': candidate.sdpMid,
                        'sdpMLineIndex': candidate.sdpMLineIndex
                    }
                }))

        try:
            # 启动 WebSocket 服务器
            async with websockets.serve(
                self.handle_client,
                self.server_ip,
                self.server_port
            ):
                logger.info("WebSocket 服务器已启动")
                # 等待连接和处理
                await asyncio.Future()  # 永久运行
                
        except KeyboardInterrupt:
            logger.info("收到中断信号")
        except Exception as e:
            logger.error(f"运行错误: {e}", exc_info=True)
        finally:
            logger.info("清理资源...")
            self.running = False
            await self.cleanup()
    
    async def handle_client(self, websocket, path):
        """处理客户端连接"""
        logger.info(f"客户端已连接: {websocket.remote_address}")
        self.ws = websocket
        
        try:
            # 处理 WebSocket 消息
            async for message in websocket:
                await self.handle_websocket_message(message)
            
            # 开始接收视频帧
            await self.receive_frames()
            
        except websockets.exceptions.ConnectionClosed:
            logger.info("客户端断开连接")
        except Exception as e:
            logger.error(f"处理客户端错误: {e}", exc_info=True)
        finally:
            self.ws = None

    async def cleanup(self):
        """清理资源"""
        if self.pc:
            await self.pc.close()
        if self.ws:
            await self.ws.close()
        cv2.destroyAllWindows()
        logger.info("资源清理完成")


async def main():
    parser = argparse.ArgumentParser(description='WebRTC 视频流接收端 (支持 H.265)')
    parser.add_argument('--ip', default='0.0.0.0', 
                        help='信令服务器监听 IP (default: 0.0.0.0)')
    parser.add_argument('--port', type=int, default=50061, 
                        help='信令服务器端口 (default: 50061)')
    parser.add_argument('--stun', default='106.14.31.123:3478',
                        help='STUN 服务器地址 (default: 106.14.31.123:3478)')
    parser.add_argument('--turn', default='106.14.31.123:3478',
                        help='TURN 服务器地址 (default: 106.14.31.123:3478)')
    parser.add_argument('--turn-user', default='rxjqr',
                        help='TURN 用户名 (default: rxjqr)')
    parser.add_argument('--turn-pass', default='rxjqrTurn123',
                        help='TURN 密码 (default: rxjqrTurn123)')
    parser.add_argument('--codec', default='h265',
                        choices=['h264', 'h265', 'vp8', 'vp9'],
                        help='视频编解码器 (default: h265)')
    
    args = parser.parse_args()
    
    receiver = VideoReceiver(
        server_ip=args.ip,
        server_port=args.port,
        stun_server=args.stun,
        turn_server=args.turn,
        turn_username=args.turn_user,
        turn_password=args.turn_pass,
        codec=args.codec
    )
    
    await receiver.run()


if __name__ == "__main__":
    print("=" * 60)
    print("WebRTC 视频接收端 (WebSocket + H.265)")
    print("=" * 60)
    print("\n配置信息:")
    print("- WebSocket 信令服务器: ws://0.0.0.0:50061")
    print("- STUN/TURN: 106.14.31.123:3478")
    print("- 支持编解码器: H.264, H.265, VP8, VP9")
    print("\n等待发送端连接...")
    print("按 'q' 键退出\n")
    
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n程序已退出")
