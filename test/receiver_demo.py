"""
WebRTC 接收端 Demo - Python 版本
用于接收来自 C++ 客户端的视频流
支持 H.264/HEVC 解码

依赖安装：
pip install aiortc opencv-python numpy websockets av

运行方式：
python receiver_demo.py --server-ip 106.14.31.123 --client-id receiver_001
"""

import asyncio
import cv2
import numpy as np
import json
from aiohttp import web
from aiortc import (
    RTCPeerConnection, 
    RTCSessionDescription, 
    RTCIceCandidate, 
    VideoStreamTrack,
    RTCConfiguration,
    RTCIceServer
)
from aiortc.sdp import candidate_from_sdp
import websockets
from av import VideoFrame, CodecContext
import argparse
import logging

# 配置日志
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class VideoReceiver:
    def __init__(self, server_ip="106.14.31.123", server_port=50061,
                 client_id="receiver_001",
                 stun_server="106.14.31.123:3478",
                 turn_server="106.14.31.123:3478",
                 turn_username="rxjqr",
                 turn_password="rxjqrTurn123",
                 codec="h264"):
        """
        初始化视频接收器
        
        Args:
            server_ip: 信令服务器 IP
            server_port: 信令服务器端口
            client_id: 客户端 ID
            stun_server: STUN 服务器地址
            turn_server: TURN 服务器地址
            turn_username: TURN 用户名
            turn_password: TURN 密码
            codec: 视频编解码器 (h264/vp8/vp9)
        """
        self.server_ip = server_ip
        self.server_port = server_port
        self.client_id = client_id
        self.codec = codec.lower()
        self.sender_id = None  # 发送端的 ID
        
        # 配置 ICE 服务器
        self.ice_servers = [
            RTCIceServer(urls=[f"stun:{stun_server}"]),
            RTCIceServer(
                urls=[f"turn:{turn_server}"],
                username=turn_username,
                credential=turn_password
            )
        ]
        
        self.pc = None
        self.ws = None
        self.video_track = None
        self.frame_count = 0
        self.running = False
        self.latest_frame = None
        self.web_runner = None
        self.web_site = None
        
        logger.info(f"客户端 ID: {self.client_id}")
        logger.info(f"使用编解码器: {self.codec.upper()}")

    async def index(self, request):
        content = """
        <html>
        <head>
            <title>WebRTC Receiver</title>
            <style>
                body { font-family: Arial, sans-serif; text-align: center; background-color: #f0f0f0; }
                h1 { color: #333; }
                .video-container { 
                    margin: 20px auto; 
                    border: 5px solid #fff; 
                    box-shadow: 0 0 10px rgba(0,0,0,0.2); 
                    display: inline-block;
                    background-color: #000;
                    min-width: 640px;
                    min-height: 480px;
                }
                img { max-width: 100%; display: block; }
                .status { margin-top: 10px; color: #666; }
            </style>
        </head>
        <body>
            <h1>WebRTC Receiver Stream</h1>
            <div class="video-container">
                <img src="/video_feed" alt="Waiting for stream..." />
            </div>
            <div class="status">
                Receiving stream via WebRTC, displaying via MJPEG
            </div>
        </body>
        </html>
        """
        return web.Response(text=content, content_type='text/html')

    async def video_feed(self, request):
        boundary = "frame"
        response = web.StreamResponse(status=200, reason='OK', headers={
            'Content-Type': 'multipart/x-mixed-replace;boundary={}'.format(boundary)
        })
        await response.prepare(request)
        
        while self.running:
            if self.latest_frame is not None:
                try:
                    await response.write(b'--' + boundary.encode() + b'\r\n')
                    await response.write(b'Content-Type: image/jpeg\r\n')
                    await response.write(b'Content-Length: ' + str(len(self.latest_frame)).encode() + b'\r\n')
                    await response.write(b'\r\n')
                    await response.write(self.latest_frame)
                    await response.write(b'\r\n')
                    # Limit FPS for browser display (approx 30 FPS)
                    await asyncio.sleep(0.033)
                except Exception:
                    break
            else:
                await asyncio.sleep(0.1)
        return response

    async def start_web_server(self, port=8080):
        app = web.Application()
        app.router.add_get('/', self.index)
        app.router.add_get('/video_feed', self.video_feed)
        
        self.web_runner = web.AppRunner(app)
        await self.web_runner.setup()
        self.web_site = web.TCPSite(self.web_runner, '0.0.0.0', port)
        await self.web_site.start()
        logger.info(f"Web server started at http://localhost:{port}")

    async def receive_frames(self):
        """接收并显示视频帧（支持 H.264 解码）"""
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
                        
                        # Encode to JPEG for web streaming
                        ret, buffer = cv2.imencode('.jpg', img)
                        if ret:
                            self.latest_frame = buffer.tobytes()
                            
                        if self.frame_count % 30 == 0:
                            logger.info(f"✅ 已接收 {self.frame_count} 帧 ({self.codec.upper()})")
                            
                    except asyncio.TimeoutError:
                        # 超时不是错误，继续等待
                        continue
                    except Exception as e:
                        logger.error(f"接收帧错误: {e}", exc_info=True)
                        # 不要break，继续尝试
                        await asyncio.sleep(0.1)
                else:
                    # 等待视频轨道准备好
                    await asyncio.sleep(0.1)
                    
        finally:
            pass

    async def handle_websocket_message(self, message):
        """处理 WebSocket 消息"""
        data = json.loads(message)
        msg_type = data.get('type')
        
        if msg_type == 'registered':
            logger.info(f"✅ 已注册: {data.get('client_id')}")
            
        elif msg_type == 'offer':
            # 保存发送端 ID
            self.sender_id = data.get('from')
            logger.info(f"📥 收到来自 {self.sender_id} 的 offer")
            
            # 设置远端描述
            offer = RTCSessionDescription(sdp=data['sdp'], type='offer')
            
            try:
                await self.pc.setRemoteDescription(offer)
            except Exception as e:
                logger.error(f"设置远端描述失败: {e}")
                raise
            
            # 创建 answer
            answer = await self.pc.createAnswer()
            await self.pc.setLocalDescription(answer)
            
            # 发送 answer (回复给发送端)
            await self.ws.send(json.dumps({
                'type': 'answer',
                'sdp': self.pc.localDescription.sdp,
                'target_id': self.sender_id  # 指定目标
            }))
            logger.info(f"📤 已发送 answer 到 {self.sender_id}")
            
        elif msg_type == 'candidate':
            logger.debug("📥 收到 ICE candidate")
            candidate_data = data['candidate']
            candidate = candidate_from_sdp(candidate_data['candidate'])
            candidate.sdpMid = candidate_data.get('sdpMid')
            candidate.sdpMLineIndex = candidate_data.get('sdpMLineIndex')
            await self.pc.addIceCandidate(candidate)
    
    def _add_video_codec_to_sdp(self, sdp):
        """添加默认视频编解码器到 SDP（如果缺失）"""
        lines = sdp.split('\r\n')
        modified = False
        new_lines = []
        in_video_section = False
        has_rtpmap = False
        
        for i, line in enumerate(lines):
            new_lines.append(line)
            
            # 检测视频媒体部分
            if line.startswith('m=video'):
                in_video_section = True
                has_rtpmap = False
            elif line.startswith('m='):
                in_video_section = False
            
            # 检查是否已有编解码器
            if in_video_section and line.startswith('a=rtpmap:'):
                has_rtpmap = True
            
            # 如果视频段结束但没有编解码器，添加 VP8
            if in_video_section and (line.startswith('m=') or i == len(lines) - 1):
                if not has_rtpmap and not modified:
                    logger.info("SDP 中缺少视频编解码器，添加 VP8")
                    # 在 m= 行后添加 VP8 编解码器
                    insert_index = new_lines.index(line) if line.startswith('m=') else len(new_lines)
                    new_lines.insert(insert_index, 'a=rtpmap:96 VP8/90000')
                    new_lines.insert(insert_index + 1, 'a=rtcp-fb:96 nack')
                    new_lines.insert(insert_index + 2, 'a=rtcp-fb:96 nack pli')
                    new_lines.insert(insert_index + 3, 'a=rtcp-fb:96 goog-remb')
                    modified = True
        
        return '\r\n'.join(new_lines) if modified else sdp

    async def run(self):
        """运行接收器（作为客户端连接到信令服务器）"""
        self.running = True
        
        # 启动 Web 服务器
        await self.start_web_server(port=8080)
        
        # 创建 PeerConnection
        self.pc = RTCPeerConnection(
            configuration=RTCConfiguration(iceServers=self.ice_servers)
        )
        
        logger.info(f"连接到信令服务器 ws://{self.server_ip}:{self.server_port}")
        
        @self.pc.on("track")
        async def on_track(track):
            logger.info(f"📹 接收到媒体轨道: {track.kind}")
            if track.kind == "video":
                self.video_track = track
                logger.info("✅ 视频轨道已就绪")

        @self.pc.on("connectionstatechange")
        async def on_connectionstatechange():
            logger.info(f"🔗 连接状态: {self.pc.connectionState}")
            if self.pc.connectionState == "connected":
                logger.info("✅ WebRTC 连接成功建立！")

        @self.pc.on("iceconnectionstatechange")
        async def on_iceconnectionstatechange():
            logger.info(f"🧊 ICE 连接状态: {self.pc.iceConnectionState}")
            if self.pc.iceConnectionState in ["completed", "connected"]:
                logger.info("✅ ICE 连接已建立，视频流应该开始传输")
        
        @self.pc.on("icecandidate")
        async def on_icecandidate(candidate):
            if candidate and self.ws:
                await self.ws.send(json.dumps({
                    'type': 'candidate',
                    'candidate': {
                        'candidate': candidate.candidate,
                        'sdpMid': candidate.sdpMid,
                        'sdpMLineIndex': candidate.sdpMLineIndex
                    },
                    'target_id': self.sender_id  # 发送给发送端
                }))

        try:
            # 连接到 WebSocket 信令服务器
            async with websockets.connect(f"ws://{self.server_ip}:{self.server_port}") as websocket:
                self.ws = websocket
                logger.info("✅ 已连接到信令服务器")
                
                # 注册
                await self.ws.send(json.dumps({
                    'type': 'register',
                    'client_id': self.client_id
                }))
                
                # 启动视频接收任务
                video_task = asyncio.create_task(self.receive_frames())
                
                # 处理 WebSocket 消息
                async for message in websocket:
                    await self.handle_websocket_message(message)
                    
        except KeyboardInterrupt:
            logger.info("收到中断信号")
        except Exception as e:
            logger.error(f"运行错误: {e}", exc_info=True)
        finally:
            logger.info("清理资源...")
            self.running = False
            await self.cleanup()

    async def cleanup(self):
        """清理资源"""
        if self.pc:
            await self.pc.close()
        if self.ws:
            await self.ws.close()
        if self.web_site:
            await self.web_site.stop()
        if self.web_runner:
            await self.web_runner.cleanup()
        logger.info("资源清理完成")


async def main():
    parser = argparse.ArgumentParser(description='WebRTC 视频流接收端')
    parser.add_argument('--server-ip', default='106.14.31.123', 
                        help='信令服务器 IP (default: 106.14.31.123)')
    parser.add_argument('--server-port', type=int, default=50061, 
                        help='信令服务器端口 (default: 50061)')
    parser.add_argument('--client-id', default='receiver_001',
                        help='客户端 ID (default: receiver_001)')
    parser.add_argument('--stun', default='106.14.31.123:3478',
                        help='STUN 服务器地址 (default: 106.14.31.123:3478)')
    parser.add_argument('--turn', default='106.14.31.123:3478',
                        help='TURN 服务器地址 (default: 106.14.31.123:3478)')
    parser.add_argument('--turn-user', default='rxjqr',
                        help='TURN 用户名 (default: rxjqr)')
    parser.add_argument('--turn-pass', default='rxjqrTurn123',
                        help='TURN 密码 (default: rxjqrTurn123)')
    parser.add_argument('--codec', default='h264',
                        choices=['h264', 'vp8', 'vp9'],
                        help='视频编解码器 (default: h264)')
    
    args = parser.parse_args()
    
    receiver = VideoReceiver(
        server_ip=args.server_ip,
        server_port=args.server_port,
        client_id=args.client_id,
        stun_server=args.stun,
        turn_server=args.turn,
        turn_username=args.turn_user,
        turn_password=args.turn_pass,
        codec=args.codec
    )
    
    await receiver.run()


if __name__ == "__main__":
    print("=" * 60)
    print("WebRTC 视频接收端")
    print("=" * 60)
    print("\n使用方法:")
    print("python receiver_demo.py --server-ip 106.14.31.123 --client-id receiver_001")
    print("\nWeb 界面: http://localhost:8080")
    print("按 Ctrl+C 退出\n")
    
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n程序已退出")
