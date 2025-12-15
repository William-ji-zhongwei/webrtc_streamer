#include "webrtc_client.h"
#include "custom_video_source.h"
#include "simple_video_codec_factory.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

// WebRTC headers
#include <api/create_peerconnection_factory.h>
#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/video_codecs/builtin_video_decoder_factory.h>
#include <api/video_codecs/builtin_video_encoder_factory.h>
#include <api/peer_connection_interface.h>
#include <rtc_base/ssl_adapter.h>
#include <rtc_base/thread.h>
#include <rtc_base/logging.h>
#include <pc/video_track_source.h>

// Observer classes
class PeerConnectionObserver : public webrtc::PeerConnectionObserver {
public:
    explicit PeerConnectionObserver(WebRTCClient* client) : client_(client) {}
    
    void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state) override {
        RTC_LOG(LS_INFO) << "Signaling state: " << new_state;
    }
    
    void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override {
        RTC_LOG(LS_INFO) << "Data channel created";
    }
    
    void OnRenegotiationNeeded() override {
        RTC_LOG(LS_INFO) << "Renegotiation needed";
    }
    
    void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state) override {
        const char* state_str[] = {
            "new", "checking", "connected", "completed", 
            "failed", "disconnected", "closed"
        };
        int state_idx = static_cast<int>(new_state);
        std::cout << "🧊 ICE connection state: " << state_str[state_idx] << std::endl;
        
        if (new_state == webrtc::PeerConnectionInterface::kIceConnectionConnected) {
            std::cout << "✅ ICE connection established!" << std::endl;
            client_->OnConnectionChange(true);
        } else if (new_state == webrtc::PeerConnectionInterface::kIceConnectionFailed) {
            std::cout << "❌ ICE connection failed! Check TURN server configuration." << std::endl;
            client_->OnConnectionChange(false);
        } else if (new_state == webrtc::PeerConnectionInterface::kIceConnectionDisconnected) {
            std::cout << "⚠️  ICE connection disconnected" << std::endl;
        } else if (new_state == webrtc::PeerConnectionInterface::kIceConnectionClosed) {
            std::cout << "⚠️  ICE connection closed" << std::endl;
            client_->OnConnectionChange(false);
        }
    }
    
    void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override {
        const char* state_str[] = {"new", "gathering", "complete"};
        int state_idx = static_cast<int>(new_state);
        std::cout << "🔍 ICE gathering state: " << state_str[state_idx] << std::endl;
    }
    
    void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
        std::string sdp;
        candidate->ToString(&sdp);
        // 显示 candidate 类型（host/srflx/relay）
        if (sdp.find("typ host") != std::string::npos) {
            std::cout << "📡 ICE candidate (host): local network" << std::endl;
        } else if (sdp.find("typ srflx") != std::string::npos) {
            std::cout << "📡 ICE candidate (srflx): via STUN" << std::endl;
        } else if (sdp.find("typ relay") != std::string::npos) {
            std::cout << "📡 ICE candidate (relay): via TURN ✅" << std::endl;
        }
        client_->OnIceCandidate(candidate);
    }
    
    void OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) override {
        RTC_LOG(LS_INFO) << "Track added";
    }
    
private:
    WebRTCClient* client_;
};

class CreateSessionDescriptionObserver : public webrtc::CreateSessionDescriptionObserver {
public:
    explicit CreateSessionDescriptionObserver(WebRTCClient* client) : client_(client) {}
    
    void OnSuccess(webrtc::SessionDescriptionInterface* desc) override {
        client_->OnOfferCreated(desc);
    }
    
    void OnFailure(webrtc::RTCError error) override {
        RTC_LOG(LS_ERROR) << "Create session description failed: " << error.message();
    }
    
private:
    WebRTCClient* client_;
};

class SetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver {
public:
    explicit SetSessionDescriptionObserver(WebRTCClient* client) : client_(client) {}
    
    void OnSuccess() override {
        client_->OnAnswerSet();
    }
    
    void OnFailure(webrtc::RTCError error) override {
        RTC_LOG(LS_ERROR) << "Set session description failed: " << error.message();
    }
    
private:
    WebRTCClient* client_;
};

// Helper to escape JSON strings
std::string escapeJsonString(const std::string& input) {
    std::ostringstream ss;
    for (char c : input) {
        switch (c) {
            case '"': ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
                } else {
                    ss << c;
                }
        }
    }
    return ss.str();
}

// WebSocket helper functions
std::string encodeWebSocketFrame(const std::string& message) {
    std::string frame;
    size_t length = message.length();
    
    frame.push_back(0x81);  // FIN bit + text frame
    
    if (length <= 125) {
        frame.push_back(static_cast<char>(length | 0x80));  // Masked
    } else if (length <= 65535) {
        frame.push_back(126 | 0x80);
        frame.push_back((length >> 8) & 0xFF);
        frame.push_back(length & 0xFF);
    }
    
    // Masking key (simplified - should be random)
    char mask[4] = {0x12, 0x34, 0x56, 0x78};
    frame.append(mask, 4);
    
    // Apply mask to payload
    for (size_t i = 0; i < length; i++) {
        frame.push_back(message[i] ^ mask[i % 4]);
    }
    
    return frame;
}

std::string decodeWebSocketFrame(const char* data, size_t len) {
    if (len < 2) return "";
    
    // 检查 opcode（第一个字节的低 4 位）
    unsigned char opcode = data[0] & 0x0F;
    
    // 0x8 = close, 0x9 = ping, 0xA = pong
    if (opcode == 0x8) {
        // Close frame
        return "";
    } else if (opcode == 0x9 || opcode == 0xA) {
        // Ping/Pong frame - 自动回复 pong（如果是 ping）
        if (opcode == 0x9) {
            // 这是 ping，应该回复 pong（在接收消息的地方处理）
        }
        return "";  // 不返回 ping/pong 内容
    }
    
    // 检查是否有 mask（第二个字节的最高位）
    bool is_masked = (data[1] & 0x80) != 0;
    size_t payload_len = data[1] & 0x7F;
    size_t pos = 2;
    
    // 处理扩展 payload 长度
    if (payload_len == 126) {
        if (len < 4) return "";
        payload_len = (static_cast<unsigned char>(data[2]) << 8) | 
                      static_cast<unsigned char>(data[3]);
        pos = 4;
    } else if (payload_len == 127) {
        if (len < 10) return "";
        // 64-bit 长度（通常不需要，但为了完整性）
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | static_cast<unsigned char>(data[2 + i]);
        }
        pos = 10;
    }
    
    // 处理 mask key
    char mask[4] = {0};
    if (is_masked) {
        if (len < pos + 4) return "";
        memcpy(mask, data + pos, 4);
        pos += 4;
    }
    
    // 检查是否有足够的数据
    if (len < pos + payload_len) return "";
    
    // 解码 payload
    std::string payload;
    payload.reserve(payload_len);
    
    if (is_masked) {
        for (size_t i = 0; i < payload_len; i++) {
            payload.push_back(data[pos + i] ^ mask[i % 4]);
        }
    } else {
        // 服务器发送的消息通常不 mask
        payload.assign(data + pos, payload_len);
    }
    
    return payload;
}

// WebRTCClient implementation
WebRTCClient::WebRTCClient(std::shared_ptr<VideoSource> video_source,
                           const WebRTCConfig& webrtc_config)
    : video_source_(video_source), webrtc_config_(webrtc_config),
      is_streaming_(false), should_stop_(false), peer_connected_(false),
      ws_socket_(-1), frame_count_(0) {
}

WebRTCClient::~WebRTCClient() {
    stop();
}

bool WebRTCClient::initialize() {
    if (!video_source_ || !video_source_->isReady()) {
        std::cerr << "Video source is not ready" << std::endl;
        return false;
    }
    
    std::cout << "Initializing WebRTC client..." << std::endl;
    
    // Initialize SSL
    rtc::InitializeSSL();
    
    // Create threads
    std::unique_ptr<rtc::Thread> network_thread = rtc::Thread::CreateWithSocketServer();
    std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
    std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
    
    network_thread->Start();
    worker_thread->Start();
    signaling_thread->Start();
    
    // Create PeerConnectionFactory
    peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
        network_thread.get(),
        worker_thread.get(),
        signaling_thread.get(),
        nullptr,
        webrtc::CreateBuiltinAudioEncoderFactory(),
        webrtc::CreateBuiltinAudioDecoderFactory(),
        std::make_unique<webrtc::SimpleVideoEncoderFactory>(),
        std::make_unique<webrtc::SimpleVideoDecoderFactory>(),
        nullptr, nullptr
    );
    
    if (!peer_connection_factory_) {
        std::cerr << "Failed to create PeerConnectionFactory" << std::endl;
        return false;
    }
    
    std::cout << "✅ WebRTC initialized successfully" << std::endl;
    
    // Keep threads alive
    network_thread.release();
    worker_thread.release();
    signaling_thread.release();
    
    return true;
}

bool WebRTCClient::createPeerConnection() {
    webrtc::PeerConnectionInterface::RTCConfiguration config;
    
    // 配置 ICE 传输类型：允许使用所有类型（包括 TURN）
    config.type = webrtc::PeerConnectionInterface::kAll;
    
    // ICE 候选过滤策略：允许所有候选
    config.candidate_network_policy = 
        webrtc::PeerConnectionInterface::kCandidateNetworkPolicyAll;
    
    // 持续收集 ICE candidates
    config.continual_gathering_policy = 
        webrtc::PeerConnectionInterface::GATHER_CONTINUALLY;
    
    // Bundle policy - 使用最大 bundle
    config.bundle_policy = webrtc::PeerConnectionInterface::kBundlePolicyMaxBundle;
    
    // RTCP Mux policy - 必须使用 mux
    config.rtcp_mux_policy = webrtc::PeerConnectionInterface::kRtcpMuxPolicyRequire;
    
    // ICE candidate pool size - 预分配候选池
    config.ice_candidate_pool_size = 4;
    
    // Add ICE servers
    for (const auto& ice_server : webrtc_config_.ice_servers) {
        webrtc::PeerConnectionInterface::IceServer server;
        server.urls = ice_server.urls;
        
        if (!ice_server.username.empty()) {
            server.username = ice_server.username;
            server.password = ice_server.credential;
            std::cout << "🔐 Adding TURN server: " << ice_server.urls[0] 
                      << " (user: " << ice_server.username << ")" << std::endl;
        } else {
            std::cout << "🌐 Adding STUN server: " << ice_server.urls[0] << std::endl;
        }
        
        config.servers.push_back(server);
    }
    
    // Create observer
    pc_observer_ = std::make_shared<PeerConnectionObserver>(this);
    
    // Create PeerConnection
    peer_connection_ = peer_connection_factory_->CreatePeerConnection(
        config, nullptr, nullptr, pc_observer_.get()
    );
    
    if (!peer_connection_) {
        std::cerr << "Failed to create PeerConnection" << std::endl;
        return false;
    }
    
    std::cout << "✅ PeerConnection created" << std::endl;
    return true;
}

bool WebRTCClient::addVideoTrack() {
    // Create custom video source
    custom_video_source_ = new rtc::RefCountedObject<CustomVideoSource>();
    
    // Create video track
    rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track =
        peer_connection_factory_->CreateVideoTrack(
            "video_track", 
            custom_video_source_.get()
        );
    
    if (!video_track) {
        std::cerr << "Failed to create video track" << std::endl;
        return false;
    }
    
    // Add track to peer connection
    auto result = peer_connection_->AddTrack(video_track, {"stream_id"});
    if (!result.ok()) {
        std::cerr << "Failed to add track: " << result.error().message() << std::endl;
        return false;
    }
    
    video_track_ = video_track;
    std::cout << "✅ Video track added" << std::endl;
    
    return true;
}

void WebRTCClient::createOffer() {
    webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
    // 发送端：只发送视频，不接收
    options.offer_to_receive_video = 0;  // 明确设置为 0（不接收）
    options.offer_to_receive_audio = 0;  // 明确设置为 0（不接收）
    
    std::cout << "📤 Creating offer (sendonly mode)" << std::endl;
    
    rtc::scoped_refptr<CreateSessionDescriptionObserver> observer(
        new rtc::RefCountedObject<CreateSessionDescriptionObserver>(this)
    );
    
    peer_connection_->CreateOffer(observer.get(), options);
}

void WebRTCClient::OnOfferCreated(webrtc::SessionDescriptionInterface* desc) {
    // 获取 SDP 字符串
    std::string sdp;
    desc->ToString(&sdp);
    
    // 打印原始 Offer SDP
    std::cout << "📤 Offer SDP (before modification):" << std::endl;
    std::cout << sdp << std::endl;
    
    // 确保 SDP 中设置为 sendonly
    // 查找并替换 a=sendrecv 或 a=recvonly 为 a=sendonly
    size_t pos = 0;
    while ((pos = sdp.find("a=sendrecv", pos)) != std::string::npos) {
        sdp.replace(pos, 10, "a=sendonly");
        std::cout << "✏️  Modified: sendrecv → sendonly" << std::endl;
        pos += 10;
    }
    
    pos = 0;
    while ((pos = sdp.find("a=recvonly", pos)) != std::string::npos) {
        sdp.replace(pos, 10, "a=sendonly");
        std::cout << "✏️  Modified: recvonly → sendonly" << std::endl;
        pos += 10;
    }
    
    // 如果没有任何方向属性，添加 sendonly
    if (sdp.find("a=sendonly") == std::string::npos && 
        sdp.find("a=recvonly") == std::string::npos &&
        sdp.find("a=sendrecv") == std::string::npos) {
        
        // 在第一个 m= 行之后添加 a=sendonly
        size_t m_line = sdp.find("m=video");
        if (m_line != std::string::npos) {
            size_t next_line = sdp.find("\r\n", m_line);
            if (next_line != std::string::npos) {
                sdp.insert(next_line + 2, "a=sendonly\r\n");
                std::cout << "➕ Added: a=sendonly" << std::endl;
            }
        }
    }
    
    // 重新创建 SessionDescription
    webrtc::SdpParseError error;
    std::unique_ptr<webrtc::SessionDescriptionInterface> modified_desc =
        webrtc::CreateSessionDescription(webrtc::SdpType::kOffer, sdp, &error);
    
    if (!modified_desc) {
        std::cerr << "❌ Failed to parse modified SDP: " << error.description << std::endl;
        return;
    }
    
    std::cout << "📤 Offer SDP (after modification):" << std::endl;
    std::string final_sdp;
    modified_desc->ToString(&final_sdp);
    std::cout << final_sdp << std::endl;
    
    // Set local description
    rtc::scoped_refptr<SetSessionDescriptionObserver> observer(
        new rtc::RefCountedObject<SetSessionDescriptionObserver>(this)
    );
    
    peer_connection_->SetLocalDescription(observer.get(), modified_desc.release());
    
    // Send offer via WebSocket
    std::ostringstream json;
    json << "{\"type\":\"offer\",\"sdp\":\"" << escapeJsonString(final_sdp) << "\"";
    
    // 如果指定了目标 ID，添加到消息中
    if (!webrtc_config_.target_id.empty()) {
        json << ",\"target_id\":\"" << webrtc_config_.target_id << "\"";
        std::cout << "📤 Sending offer to: " << webrtc_config_.target_id << std::endl;
    } else {
        std::cout << "📤 Broadcasting offer to all receivers" << std::endl;
    }
    
    json << "}";
    
    sendMessage(json.str());
}

void WebRTCClient::OnAnswerSet() {
    std::cout << "✅ Answer set successfully" << std::endl;
}

void WebRTCClient::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
    std::string sdp;
    candidate->ToString(&sdp);
    
    std::ostringstream json;
    json << "{\"type\":\"candidate\",\"candidate\":{"
         << "\"candidate\":\"" << escapeJsonString(sdp) << "\","
         << "\"sdpMid\":\"" << escapeJsonString(candidate->sdp_mid()) << "\","
         << "\"sdpMLineIndex\":" << candidate->sdp_mline_index()
         << "}}";
    
    sendMessage(json.str());
    RTC_LOG(LS_INFO) << "ICE candidate sent";
}

void WebRTCClient::OnConnectionChange(bool connected) {
    peer_connected_ = connected;
    if (connected) {
        std::cout << "✅ WebRTC peer connected!" << std::endl;
    } else {
        std::cout << "⚠️  WebRTC peer disconnected" << std::endl;
    }
}

bool WebRTCClient::start() {
    if (is_streaming_) {
        return false;
    }
    
    should_stop_ = false;
    is_streaming_ = true;
    
    // Start threads
    signaling_thread_ = std::thread(&WebRTCClient::signalingThread, this);
    streaming_thread_ = std::thread(&WebRTCClient::captureAndEncodeFrames, this);
    
    std::cout << "🚀 Streaming started" << std::endl;
    return true;
}

void WebRTCClient::stop() {
    if (!is_streaming_) {
        return;
    }
    
    should_stop_ = true;
    is_streaming_ = false;
    queue_cv_.notify_all();
    
    if (signaling_thread_.joinable()) {
        signaling_thread_.join();
    }
    
    if (streaming_thread_.joinable()) {
        streaming_thread_.join();
    }
    
    if (peer_connection_) {
        peer_connection_->Close();
        peer_connection_ = nullptr;
    }
    
    if (ws_socket_ >= 0) {
        close(ws_socket_);
        ws_socket_ = -1;
    }
    
    rtc::CleanupSSL();
    
    std::cout << "Streaming stopped" << std::endl;
}

void WebRTCClient::captureAndEncodeFrames() {
    std::cout << "Capture thread started" << std::endl;
    
    auto frame_duration = std::chrono::milliseconds(33);  // ~30 fps
    auto next_frame_time = std::chrono::steady_clock::now();
    
    while (!should_stop_) {
        cv::Mat frame;
        if (video_source_->getFrame(frame) && !frame.empty()) {
            frame_count_++;
            
            // Add timestamp
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;
            
            char timestamp[100];
            std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", 
                         std::localtime(&time_t_now));
            sprintf(timestamp + strlen(timestamp), ".%03d", static_cast<int>(ms.count()));
            
            cv::putText(frame, timestamp, cv::Point(10, 30),
                       cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
            
            // Push to WebRTC video source
            if (custom_video_source_) {
                custom_video_source_->PushFrame(frame);
            }
            
            if (frame_count_ % 30 == 0) {
                std::cout << "📹 Captured " << frame_count_ << " frames" << std::endl;
            }
        }
        
        next_frame_time += frame_duration;
        std::this_thread::sleep_until(next_frame_time);
    }
    
    std::cout << "Capture thread stopped" << std::endl;
}

void WebRTCClient::sendMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(ws_mutex_);
    
    if (ws_socket_ < 0) {
        return;
    }
    
    std::string frame = encodeWebSocketFrame(message);
    send(ws_socket_, frame.c_str(), frame.length(), 0);
}

std::string WebRTCClient::receiveMessage() {
    char buffer[8192];
    ssize_t bytes = recv(ws_socket_, buffer, sizeof(buffer), 0);
    
    if (bytes <= 0) {
        return "";
    }
    
    // 检查是否是 ping frame (opcode 0x9)
    if (bytes >= 2 && (buffer[0] & 0x0F) == 0x9) {
        // 收到 ping，发送 pong 回复
        std::string pong_frame;
        pong_frame.push_back(0x8A);  // FIN bit + pong opcode
        pong_frame.push_back(0x00);  // 无 payload，无 mask
        
        std::lock_guard<std::mutex> lock(ws_mutex_);
        send(ws_socket_, pong_frame.c_str(), pong_frame.length(), 0);
        
        // 返回空字符串，不处理 ping 消息
        return "";
    }
    
    // 检查是否是 pong frame (opcode 0xA)
    if (bytes >= 2 && (buffer[0] & 0x0F) == 0xA) {
        // 收到 pong，忽略
        return "";
    }
    
    // 检查是否是 close frame (opcode 0x8)
    if (bytes >= 2 && (buffer[0] & 0x0F) == 0x8) {
        std::cout << "⚠️  WebSocket close frame received" << std::endl;
        return "";
    }
    
    return decodeWebSocketFrame(buffer, bytes);
}

void WebRTCClient::signalingThread() {
    std::cout << "Signaling thread started" << std::endl;
    
    // Connect to WebSocket server
    ws_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (ws_socket_ < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(webrtc_config_.server_port);
    inet_pton(AF_INET, webrtc_config_.server_ip.c_str(), &server_addr.sin_addr);
    
    std::cout << "Connecting to " << webrtc_config_.server_ip 
              << ":" << webrtc_config_.server_port << "..." << std::endl;
    
    if (connect(ws_socket_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Failed to connect: " << strerror(errno) << std::endl;
        close(ws_socket_);
        ws_socket_ = -1;
        return;
    }
    
    // WebSocket handshake
    std::ostringstream handshake;
    handshake << "GET / HTTP/1.1\r\n"
              << "Host: " << webrtc_config_.server_ip << "\r\n"
              << "Upgrade: websocket\r\n"
              << "Connection: Upgrade\r\n"
              << "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
              << "Sec-WebSocket-Version: 13\r\n\r\n";
    
    send(ws_socket_, handshake.str().c_str(), handshake.str().length(), 0);
    
    // Receive handshake response
    char buffer[4096];
    recv(ws_socket_, buffer, sizeof(buffer), 0);
    
    std::cout << "✅ WebSocket connected" << std::endl;
    
    // Register with server
    std::ostringstream register_msg;
    register_msg << "{\"type\":\"register\",\"client_id\":\"" 
                 << webrtc_config_.client_id << "\"}";
    sendMessage(register_msg.str());
    std::cout << "📤 Registered as: " << webrtc_config_.client_id << std::endl;
    
    // Wait for registration confirmation
    std::string reg_response = receiveMessage();
    std::cout << "📥 Server response: " << reg_response << std::endl;
    
    // Create PeerConnection and add video track
    if (!createPeerConnection() || !addVideoTrack()) {
        return;
    }
    
    // Create and send offer
    createOffer();
    
    // Receive messages
    while (!should_stop_) {
        std::string message = receiveMessage();
        
        if (message.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // 检查是否是 keepalive 或其他控制消息
        if (message.find("keepalive") != std::string::npos || 
            message.find("ping") != std::string::npos ||
            message.find("pong") != std::string::npos) {
            // 不显示 keepalive 消息，避免日志污染
            continue;
        }
        
        std::cout << "📥 Received: " << message.substr(0, 100);
        if (message.length() > 100) std::cout << "...";
        std::cout << std::endl;

        // Parse JSON (simplified)
        if (message.find("\"type\"") != std::string::npos && 
            message.find("answer") != std::string::npos) {
            
            // Extract SDP
            size_t sdp_key_pos = message.find("\"sdp\"");
            if (sdp_key_pos == std::string::npos) continue;
            
            size_t sdp_start_quote = message.find("\"", sdp_key_pos + 5);
            if (sdp_start_quote == std::string::npos) continue;
            
            size_t sdp_end_quote = message.find("\"", sdp_start_quote + 1);
            while (sdp_end_quote != std::string::npos && message[sdp_end_quote-1] == '\\') {
                 sdp_end_quote = message.find("\"", sdp_end_quote + 1);
            }
            if (sdp_end_quote == std::string::npos) continue;
            
            std::string sdp = message.substr(sdp_start_quote + 1, sdp_end_quote - sdp_start_quote - 1);
            
            // Unescape newlines
            std::string unescaped_sdp;
            for (size_t i = 0; i < sdp.length(); ++i) {
                if (sdp[i] == '\\' && i + 1 < sdp.length()) {
                    if (sdp[i+1] == 'r') { unescaped_sdp += '\r'; i++; }
                    else if (sdp[i+1] == 'n') { unescaped_sdp += '\n'; i++; }
                    else { unescaped_sdp += sdp[i]; }
                } else {
                    unescaped_sdp += sdp[i];
                }
            }
            
            // Create answer
            webrtc::SdpParseError error;
            std::unique_ptr<webrtc::SessionDescriptionInterface> answer =
                webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, unescaped_sdp, &error);
            
            if (answer) {
                std::string answer_sdp;
                answer->ToString(&answer_sdp);
                std::cout << "📥 Answer SDP:\n" << answer_sdp << std::endl;

                rtc::scoped_refptr<SetSessionDescriptionObserver> observer(
                    new rtc::RefCountedObject<SetSessionDescriptionObserver>(this)
                );
                peer_connection_->SetRemoteDescription(observer.get(), answer.release());
                std::cout << "✅ Answer received and set" << std::endl;
            }
        }
    }
    
    std::cout << "Signaling thread stopped" << std::endl;
}
