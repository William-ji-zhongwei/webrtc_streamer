#include "webrtc_client.h"
#include "custom_video_source.h"
#include "h265_encoder.h"
#include <iostream>
#include <sstream>
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
        RTC_LOG(LS_INFO) << "ICE connection state: " << new_state;
        if (new_state == webrtc::PeerConnectionInterface::kIceConnectionConnected) {
            client_->OnConnectionChange(true);
        } else if (new_state == webrtc::PeerConnectionInterface::kIceConnectionFailed ||
                   new_state == webrtc::PeerConnectionInterface::kIceConnectionClosed) {
            client_->OnConnectionChange(false);
        }
    }
    
    void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state) override {
        RTC_LOG(LS_INFO) << "ICE gathering state: " << new_state;
    }
    
    void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {
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
    
    size_t pos = 2;
    size_t payload_len = data[1] & 0x7F;
    
    if (payload_len == 126) {
        if (len < 4) return "";
        payload_len = (static_cast<unsigned char>(data[2]) << 8) | 
                      static_cast<unsigned char>(data[3]);
        pos = 4;
    }
    
    if (len < pos + payload_len) return "";
    
    return std::string(data + pos, payload_len);
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
        nullptr,  // default ADM
        webrtc::CreateBuiltinAudioEncoderFactory(),
        webrtc::CreateBuiltinAudioDecoderFactory(),
        webrtc::CreateBuiltinVideoEncoderFactory(),
        webrtc::CreateBuiltinVideoDecoderFactory(),
        nullptr,  // audio mixer
        nullptr   // audio processing
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
    
    // Add ICE servers
    for (const auto& ice_server : webrtc_config_.ice_servers) {
        webrtc::PeerConnectionInterface::IceServer server;
        server.urls = ice_server.urls;
        
        if (!ice_server.username.empty()) {
            server.username = ice_server.username;
            server.password = ice_server.credential;
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
    custom_video_source_ = std::make_shared<CustomVideoSource>();
    
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
    options.offer_to_receive_video = false;
    options.offer_to_receive_audio = false;
    
    rtc::scoped_refptr<CreateSessionDescriptionObserver> observer =
        new rtc::RefCountedObject<CreateSessionDescriptionObserver>(this);
    
    peer_connection_->CreateOffer(observer, options);
}

void WebRTCClient::OnOfferCreated(webrtc::SessionDescriptionInterface* desc) {
    // Set local description
    rtc::scoped_refptr<SetSessionDescriptionObserver> observer =
        new rtc::RefCountedObject<SetSessionDescriptionObserver>(this);
    
    peer_connection_->SetLocalDescription(observer, desc);
    
    // Send offer via WebSocket
    std::string sdp;
    desc->ToString(&sdp);
    
    std::ostringstream json;
    json << "{\"type\":\"offer\",\"sdp\":\"" << sdp << "\"}";
    
    sendMessage(json.str());
    std::cout << "📤 Offer sent" << std::endl;
}

void WebRTCClient::OnAnswerSet() {
    std::cout << "✅ Answer set successfully" << std::endl;
}

void WebRTCClient::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
    std::string sdp;
    candidate->ToString(&sdp);
    
    std::ostringstream json;
    json << "{\"type\":\"candidate\",\"candidate\":{"
         << "\"candidate\":\"" << sdp << "\","
         << "\"sdpMid\":\"" << candidate->sdp_mid() << "\","
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
        cv::Mat frame = video_source_->getFrame();
        
        if (!frame.empty()) {
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
            if (custom_video_source_ && peer_connected_) {
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
        
        // Parse JSON (simplified)
        if (message.find("\"type\":\"answer\"") != std::string::npos) {
            // Extract SDP
            size_t sdp_start = message.find("\"sdp\":\"") + 7;
            size_t sdp_end = message.find("\"", sdp_start);
            std::string sdp = message.substr(sdp_start, sdp_end - sdp_start);
            
            // Create answer
            webrtc::SdpParseError error;
            std::unique_ptr<webrtc::SessionDescriptionInterface> answer =
                webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, sdp, &error);
            
            if (answer) {
                rtc::scoped_refptr<SetSessionDescriptionObserver> observer =
                    new rtc::RefCountedObject<SetSessionDescriptionObserver>(this);
                peer_connection_->SetRemoteDescription(observer, answer.release());
                std::cout << "📥 Answer received and set" << std::endl;
            }
        }
    }
    
    std::cout << "Signaling thread stopped" << std::endl;
}
