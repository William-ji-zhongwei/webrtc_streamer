#include "webrtc_client.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

// Simple WebSocket frame encoding
std::string encodeWebSocketFrame(const std::string& message) {
    std::string frame;
    size_t length = message.length();
    
    // FIN bit + text frame
    frame.push_back(0x81);
    
    // Payload length
    if (length <= 125) {
        frame.push_back(static_cast<char>(length));
    } else if (length <= 65535) {
        frame.push_back(126);
        frame.push_back((length >> 8) & 0xFF);
        frame.push_back(length & 0xFF);
    }
    
    // Payload
    frame.append(message);
    return frame;
}

// Simple WebSocket frame decoding
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
    
    // Check if masked (from client)
    bool masked = (data[1] & 0x80) != 0;
    char mask[4] = {0};
    
    if (masked) {
        if (len < pos + 4) return "";
        memcpy(mask, data + pos, 4);
        pos += 4;
    }
    
    if (len < pos + payload_len) return "";
    
    std::string payload;
    for (size_t i = 0; i < payload_len; i++) {
        char c = data[pos + i];
        if (masked) {
            c ^= mask[i % 4];
        }
        payload.push_back(c);
    }
    
    return payload;
}

WebRTCClient::WebRTCClient(std::shared_ptr<VideoSource> video_source,
                           const WebRTCConfig& webrtc_config)
    : video_source_(video_source), webrtc_config_(webrtc_config),
      is_streaming_(false), should_stop_(false), frame_count_(0) {
}

WebRTCClient::~WebRTCClient() {
    stop();
}

bool WebRTCClient::initialize() {
    if (!video_source_ || !video_source_->isReady()) {
        std::cerr << "Video source is not ready" << std::endl;
        return false;
    }
    
    std::cout << "WebRTC client initialized" << std::endl;
    std::cout << "Server: " << webrtc_config_.server_ip << ":" << webrtc_config_.server_port << std::endl;
    std::cout << "Video source: " << video_source_->getName() << std::endl;
    
    std::cout << "\nICE Servers configuration:" << std::endl;
    for (size_t i = 0; i < webrtc_config_.ice_servers.size(); i++) {
        const auto& ice = webrtc_config_.ice_servers[i];
        std::cout << "  [" << i + 1 << "] ";
        for (size_t j = 0; j < ice.urls.size(); j++) {
            std::cout << ice.urls[j];
            if (j < ice.urls.size() - 1) std::cout << ", ";
        }
        if (!ice.username.empty()) {
            std::cout << " (authenticated)";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
    
    return true;
}

bool WebRTCClient::start() {
    if (is_streaming_) {
        std::cerr << "Already streaming" << std::endl;
        return false;
    }
    
    should_stop_ = false;
    is_streaming_ = true;
    frame_count_ = 0;
    
    // Start streaming thread
    streaming_thread_ = std::thread(&WebRTCClient::streamingThread, this);
    
    // Start signaling thread (for WebRTC handshake)
    signaling_thread_ = std::thread(&WebRTCClient::signalingThread, this);
    
    std::cout << "Streaming started" << std::endl;
    return true;
}

void WebRTCClient::stop() {
    if (!is_streaming_) {
        return;
    }
    
    should_stop_ = true;
    is_streaming_ = false;
    
    // Wake up threads
    queue_cv_.notify_all();
    
    // Wait for threads to finish
    if (streaming_thread_.joinable()) {
        streaming_thread_.join();
    }
    
    if (signaling_thread_.joinable()) {
        signaling_thread_.join();
    }
    
    // Clear frame queue
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!frame_queue_.empty()) {
        frame_queue_.pop();
    }
    
    std::cout << "Streaming stopped. Total frames: " << frame_count_ << std::endl;
}

void WebRTCClient::streamingThread() {
    auto frame_duration = std::chrono::milliseconds(1000 / video_source_->getFrameRate());
    auto next_frame_time = std::chrono::steady_clock::now();
    
    while (!should_stop_) {
        cv::Mat frame;
        
        // Get frame from video source
        if (!video_source_->getFrame(frame)) {
            std::cerr << "Failed to get frame from video source" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        
        // Add timestamp overlay
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S")
            << "." << std::setfill('0') << std::setw(3) << ms.count();
        
        cv::putText(frame, oss.str(), cv::Point(10, 30),
                   cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
        
        // Add to queue for encoding/transmission
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (frame_queue_.size() >= MAX_QUEUE_SIZE) {
                frame_queue_.pop(); // Drop oldest frame if queue is full
            }
            frame_queue_.push(frame.clone());
        }
        queue_cv_.notify_one();
        
        frame_count_++;
        if (frame_count_ % 30 == 0) {
            std::cout << "Sent frame " << frame_count_ << std::endl;
        }
        
        // Maintain frame rate
        next_frame_time += frame_duration;
        std::this_thread::sleep_until(next_frame_time);
    }
}

void WebRTCClient::signalingThread() {
    std::cout << "WebSocket 信令线程已启动" << std::endl;
    
    // 创建 socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "创建 socket 失败" << std::endl;
        return;
    }
    
    // 连接到 WebSocket 服务器
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(webrtc_config_.server_port);
    
    if (inet_pton(AF_INET, webrtc_config_.server_ip.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "无效的服务器地址: " << webrtc_config_.server_ip << std::endl;
        close(sock);
        return;
    }
    
    std::cout << "正在连接到 WebSocket 服务器 " 
              << webrtc_config_.server_ip << ":" 
              << webrtc_config_.server_port << "..." << std::endl;
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "连接失败: " << strerror(errno) << std::endl;
        close(sock);
        return;
    }
    
    std::cout << "TCP 连接已建立" << std::endl;
    
    // 发送 WebSocket 握手请求
    std::ostringstream handshake;
    handshake << "GET / HTTP/1.1\r\n"
              << "Host: " << webrtc_config_.server_ip << ":" << webrtc_config_.server_port << "\r\n"
              << "Upgrade: websocket\r\n"
              << "Connection: Upgrade\r\n"
              << "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
              << "Sec-WebSocket-Version: 13\r\n"
              << "\r\n";
    
    std::string handshake_str = handshake.str();
    if (send(sock, handshake_str.c_str(), handshake_str.length(), 0) < 0) {
        std::cerr << "发送握手失败" << std::endl;
        close(sock);
        return;
    }
    
    // 接收握手响应
    char buffer[4096];
    ssize_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received < 0) {
        std::cerr << "接收握手响应失败" << std::endl;
        close(sock);
        return;
    }
    
    buffer[bytes_received] = '\0';
    std::string response(buffer);
    
    if (response.find("101 Switching Protocols") == std::string::npos) {
        std::cerr << "WebSocket 握手失败" << std::endl;
        std::cerr << "响应: " << response << std::endl;
        close(sock);
        return;
    }
    
    std::cout << "✅ WebSocket 连接已建立" << std::endl;
    
    // 创建并发送 offer (简化版本 - 实际需要使用 WebRTC API)
    std::ostringstream offer_json;
    offer_json << "{"
               << "\"type\":\"offer\","
               << "\"sdp\":\"v=0\\r\\n"
               << "o=- 0 0 IN IP4 127.0.0.1\\r\\n"
               << "s=WebRTC Stream\\r\\n"
               << "t=0 0\\r\\n"
               << "m=video 9 UDP/TLS/RTP/SAVPF 96\\r\\n"
               << "c=IN IP4 0.0.0.0\\r\\n"
               << "a=rtcp:9 IN IP4 0.0.0.0\\r\\n";
    
    // 添加 ICE 服务器信息
    for (const auto& ice_server : webrtc_config_.ice_servers) {
        for (const auto& url : ice_server.urls) {
            offer_json << "a=ice-server:" << url << "\\r\\n";
        }
    }
    
    offer_json << "\""
               << "}";
    
    std::string offer_message = offer_json.str();
    std::string ws_frame = encodeWebSocketFrame(offer_message);
    
    std::cout << "发送 offer..." << std::endl;
    if (send(sock, ws_frame.c_str(), ws_frame.length(), 0) < 0) {
        std::cerr << "发送 offer 失败" << std::endl;
        close(sock);
        return;
    }
    
    std::cout << "等待 answer..." << std::endl;
    
    // 接收 answer 和其他消息
    while (!should_stop_) {
        bytes_received = recv(sock, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                std::cout << "服务器关闭了连接" << std::endl;
            } else {
                std::cerr << "接收数据失败: " << strerror(errno) << std::endl;
            }
            break;
        }
        
        // 解码 WebSocket 帧
        std::string message = decodeWebSocketFrame(buffer, bytes_received);
        if (!message.empty()) {
            std::cout << "收到消息: " << message.substr(0, 100);
            if (message.length() > 100) std::cout << "...";
            std::cout << std::endl;
            
            // 解析 JSON (简化版本)
            if (message.find("\"type\":\"answer\"") != std::string::npos) {
                std::cout << "✅ 收到 answer，WebRTC 连接建立中..." << std::endl;
            } else if (message.find("\"type\":\"candidate\"") != std::string::npos) {
                std::cout << "收到 ICE candidate" << std::endl;
            }
        }
        
        // 处理帧队列 (发送视频)
        cv::Mat frame;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (!frame_queue_.empty()) {
                frame = frame_queue_.front();
                frame_queue_.pop();
            }
        }
        
        if (!frame.empty()) {
            // 在真实实现中，这里应该编码并通过 WebRTC 发送帧
            // 现在只是模拟
        }
    }
    
    close(sock);
    std::cout << "WebSocket 信令线程已停止" << std::endl;
}
